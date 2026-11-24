/*
 * feishu_comment.c - C11 port of upstream
 * plugins/platforms/feishu/feishu_comment.py.
 *
 * Gates: w30_feishucomm_6493fb_p1 (event/comment/reaction payload shaping),
 *        w30_feishucomm_6493fb_p2 (rich-text segments, timeline, prompt text).
 *
 * PoP: w30_feishucomm_6493fb_p1_parse_event          @ feishu_comment.py:parse_drive_comment_event
 * PoP: w30_feishucomm_6493fb_p1_meta_result          @ feishu_comment.py:query_document_meta
 * PoP: w30_feishucomm_6493fb_p1_comment_result       @ feishu_comment.py:batch_query_comment
 * PoP: w30_feishucomm_6493fb_p1_pages_result         @ feishu_comment.py:_list_all_pages
 * PoP: w30_feishucomm_6493fb_p1_reaction_result      @ feishu_comment.py:update_comment_reaction
 * PoP: w30_feishucomm_6493fb_p1_reply_result         @ feishu_comment.py:reply_to_comment
 * PoP: w30_feishucomm_6493fb_p1_whole_comment_result @ feishu_comment.py:add_whole_comment
 * PoP: w30_feishucomm_6493fb_p1_deliver_result       @ feishu_comment.py:deliver_comment_reply
 * PoP: w30_feishucomm_6493fb_p2_sanitize             @ feishu_comment.py:_sanitize_comment_text
 * PoP: w30_feishucomm_6493fb_p2_chunk                @ feishu_comment.py:_chunk_text
 * PoP: w30_feishucomm_6493fb_p2_extract_text         @ feishu_comment.py:_extract_reply_text
 * PoP: w30_feishucomm_6493fb_p2_reply_user           @ feishu_comment.py:_get_reply_user_id
 * PoP: w30_feishucomm_6493fb_p2_reply_list           @ feishu_comment.py:_reply_list_replies
 * PoP: w30_feishucomm_6493fb_p2_extract_links        @ feishu_comment.py:_extract_docs_links
 * PoP: w30_feishucomm_6493fb_p2_format_docs          @ feishu_comment.py:_format_referenced_docs
 * PoP: w30_feishucomm_6493fb_p2_truncate             @ feishu_comment.py:_truncate
 * PoP: w30_feishucomm_6493fb_p2_select               @ feishu_comment.py:_select_timeline
 * PoP: w30_feishucomm_6493fb_p2_timeline             @ feishu_comment.py:_timeline_entry
 * PoP: w30_feishucomm_6493fb_p2_build_local          @ feishu_comment.py:build_local_comment_prompt
 * PoP: w30_feishucomm_6493fb_p2_build_whole          @ feishu_comment.py:build_whole_comment_prompt
 * PoP: w30_feishucomm_6493fb_p2_whole_prompt         @ feishu_comment.py:_whole_comment_prompt
 * PoP: w30_feishucomm_6493fb_p2_local_prompt         @ feishu_comment.py:_local_comment_prompt
 *
 * Seams (never ported here): the lark_oapi transport (_exec_request), the
 * tenant token, the wiki-node GET (a wiki link keeps its own type/token), the
 * AIAgent run and the add/delete of the OK reaction.  Every one of them enters
 * as a value: a canned (code, msg, data) triple, or replies that already
 * arrived.  What is ported is what those values decide -- the request body a
 * call carries, the retry/pagination policy around it, the plain text a
 * rich-text element list yields, the timeline window a prompt embeds, and the
 * prompt itself.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "noc_w30_feishucomm_6493fb_p1.h"
#include "noc_w30_feishucomm_6493fb_p2.h"

#define FC_REACTION_URI       "/open-apis/drive/v2/files/:file_token/comments/reaction"
#define FC_BATCH_META_URI     "/open-apis/drive/v1/metas/batch_query"
#define FC_BATCH_COMMENT_URI  "/open-apis/drive/v1/files/:file_token/comments/batch_query"
#define FC_REPLIES_URI        "/open-apis/drive/v1/files/:file_token/comments/:comment_id/replies"
#define FC_ADD_COMMENT_URI    "/open-apis/drive/v1/files/:file_token/new_comments"

#define FC_RETRY_LIMIT        6
#define FC_MAX_PAGES          5
#define FC_REPLY_CHUNK_SIZE   4000
#define FC_PROMPT_TEXT_LIMIT  220
#define FC_LOCAL_TL_LIMIT     20
#define FC_WHOLE_TL_LIMIT     12

static const char *const FC_MENTION_NOTE =
    "This comment mentioned you (@mention is for routing, not task content).";

static const char *const FC_COMMON_INSTRUCTIONS =
    "This is a Feishu document comment thread, not an IM chat.\n"
    "Do NOT call feishu_drive_add_comment or feishu_drive_reply_comment yourself.\n"
    "Your reply will be posted automatically. Just output the reply text.\n"
    "Use the thread timeline above as the main context.\n"
    "If the quoted content is not enough, use feishu_doc_read to read nearby context.\n"
    "The quoted content is your primary anchor \xe2\x80\x94 insert/summarize/explain requests are about it.\n"
    "Do not guess document content you haven't read.\n"
    "Reply in the same language as the user's comment unless they request otherwise.\n"
    "Use plain text only. Do not use Markdown, headings, bullet lists, tables, or code blocks.\n"
    "Do not show your reasoning process. Do not start with \"I will\", \"Let me\", or \"I'll first\".\n"
    "Output only the final user-facing reply.\n"
    "If no reply is needed, output exactly NO_REPLY.";

/* ------------------------------------------------------------------ */
/* string builder                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} fc_sb;

static void fc_sb_init(fc_sb *b) { b->buf = NULL; b->len = 0; b->cap = 0; }

static void fc_sb_addn(fc_sb *b, const char *s, size_t n) {
    size_t cap;
    char *nb;
    if (n == 0) return;
    if (b->len + n + 1 > b->cap) {
        cap = b->cap ? b->cap : 128;
        while (cap < b->len + n + 1) cap *= 2;
        nb = (char *)realloc(b->buf, cap);
        if (!nb) return;
        b->buf = nb;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void fc_sb_add(fc_sb *b, const char *s) {
    if (s) fc_sb_addn(b, s, strlen(s));
}

static void fc_sb_ch(fc_sb *b, char c) { fc_sb_addn(b, &c, 1); }

static void fc_sb_int(fc_sb *b, long long v) {
    char t[40];
    snprintf(t, sizeof(t), "%lld", v);
    fc_sb_add(b, t);
}

/* Take the buffer as a valid NUL-terminated malloc'd string. */
static char *fc_sb_take(fc_sb *b) {
    char *r = b->buf;
    if (!r) {
        r = (char *)malloc(1);
        if (r) r[0] = '\0';
        return r;
    }
    b->buf = NULL;
    b->len = b->cap = 0;
    return r;
}

static char *fc_dup(const char *s) {
    size_t n;
    char *r;
    if (!s) s = "";
    n = strlen(s);
    r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n + 1);
    return r;
}

/* Append one line to a newline-joined block. */
static void fc_line(fc_sb *b, const char *s, int *first) {
    if (!*first) fc_sb_ch(b, '\n');
    fc_sb_add(b, s ? s : "");
    *first = 0;
}

/* ------------------------------------------------------------------ */
/* Python value semantics                                              */
/* ------------------------------------------------------------------ */

/* Python truthiness of a JSON value. */
static int fc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(v) for the scalars a Feishu payload carries. */
static char *fc_py_str(const json_t *v) {
    char t[64];
    if (!v) return fc_dup("");
    switch (v->type) {
    case JSON_NULL:
        return fc_dup("");
    case JSON_BOOL:
        return fc_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e16 && d < 1e16) {
            snprintf(t, sizeof(t), "%lld", (long long)d);
        } else {
            snprintf(t, sizeof(t), "%.17g", d);
        }
        return fc_dup(t);
    }
    case JSON_STRING:
        return fc_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:
        return fc_dup(json_len(v) ? "[...]" : "[]");
    case JSON_OBJECT:
        return fc_dup(json_object_size(v) ? "{...}" : "{}");
    }
    return fc_dup("");
}

/* str(v or "") */
static char *fc_py_str_or_empty(const json_t *v) {
    if (!fc_truthy(v)) return fc_dup("");
    return fc_py_str(v);
}

/* obj.get(key, "") for a string field. */
static const char *fc_s(const json_t *obj, const char *key) {
    const json_t *v = json_obj_get(obj, key);
    if (!v || v->type != JSON_STRING || !v->str_val) return "";
    return v->str_val;
}

/* The string value of a bare node ("" for anything else). */
static const char *fc_vs(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* The numeric value of a bare scalar node. */
static double fc_num_of(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

static int fc_same(const char *a, const char *b) { return strcmp(a, b) == 0; }

/* ------------------------------------------------------------------ */
/* UTF-8 code-point helpers (Python len()/slicing count code points)   */
/* ------------------------------------------------------------------ */

static size_t fc_u8_len(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    if (!s) return 0;
    for (; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Byte offset of code point *cp*, clamped to the end of the string. */
static size_t fc_u8_off(const char *s, size_t cp) {
    size_t i = 0, k = 0;
    if (!s) return 0;
    while (s[i]) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) {
            if (k == cp) return i;
            k++;
        }
        i++;
    }
    return i;
}

static char *fc_prefix_bytes(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

/* First *n* code points as a fresh string. */
static char *fc_prefix_cps(const char *s, size_t n) {
    if (!s) return fc_dup("");
    return fc_prefix_bytes(s, fc_u8_off(s, n));
}

/* ------------------------------------------------------------------ */
/* predicates / text shaping                                           */
/* ------------------------------------------------------------------ */

static int fc_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* " ".join(s.split()) */
static char *fc_collapse_ws(const char *s) {
    fc_sb b;
    const char *p;
    int pending = 0, started = 0;
    fc_sb_init(&b);
    for (p = s ? s : ""; *p; p++) {
        if (fc_is_ws(*p)) {
            if (started) pending = 1;
        } else {
            if (pending) { fc_sb_ch(&b, ' '); pending = 0; }
            fc_sb_ch(&b, *p);
            started = 1;
        }
    }
    return fc_sb_take(&b);
}

/* _sanitize_comment_text */
static char *fc_sanitize(const char *text) {
    fc_sb b;
    const char *p;
    fc_sb_init(&b);
    for (p = text ? text : ""; *p; p++) {
        if (*p == '&')      fc_sb_add(&b, "&amp;");
        else if (*p == '<') fc_sb_add(&b, "&lt;");
        else if (*p == '>') fc_sb_add(&b, "&gt;");
        else                fc_sb_ch(&b, *p);
    }
    return fc_sb_take(&b);
}

/* _truncate */
static char *fc_truncate(const char *text, size_t limit) {
    fc_sb b;
    char *head;
    if (fc_u8_len(text) <= limit) return fc_dup(text);
    head = fc_prefix_cps(text, limit);
    fc_sb_init(&b);
    fc_sb_add(&b, head);
    fc_sb_add(&b, "...");
    free(head);
    return fc_sb_take(&b);
}

/* _chunk_text */
static json_t *fc_chunk_text(const char *text, size_t limit) {
    json_t *out = json_array();
    char *cur = fc_dup(text ? text : "");
    int any = 0;
    while (fc_u8_len(cur) > limit) {
        size_t pref = fc_u8_off(cur, limit);
        size_t cut = pref;
        size_t i;
        int found = 0;
        char *piece, *rest;
        if (pref > 0) {
            for (i = pref; i > 0; i--) {
                if (cur[i - 1] == '\n') { cut = i - 1; found = 1; break; }
            }
        }
        if (!found || cut == 0) cut = pref;
        piece = fc_prefix_bytes(cur, cut);
        json_append(out, json_string(piece));
        free(piece);
        rest = fc_dup(cur + cut);
        {
            size_t k = 0;
            while (rest[k] == '\n') k++;
            if (k) memmove(rest, rest + k, strlen(rest + k) + 1);
        }
        free(cur);
        cur = rest;
        any = 1;
    }
    if (cur[0] != '\0' || !any) json_append(out, json_string(cur));
    free(cur);
    return out;
}

/* ------------------------------------------------------------------ */
/* reply content -> plain text (_extract_reply_text)                   */
/* ------------------------------------------------------------------ */

/* Resolve a reply's "content": the dict itself or a JSON string holding one.
 * Returns an owned tree; *bad* is set when the string does not decode (the
 * Python then returns the raw text unchanged). */
static json_t *fc_maybe_json_owned(const json_t *v, int *bad) {
    *bad = 0;
    if (!v) return NULL;
    if (v->type == JSON_STRING) {
        json_t *p = json_parse(v->str_val ? v->str_val : "", NULL);
        if (!p) { *bad = 1; return NULL; }
        return p;
    }
    return json_copy(v);
}

/* The plain text of one rich-text element list. */
static char *fc_elements_text(const json_t *content, int semantic, const char *self_open_id) {
    const json_t *elements;
    fc_sb b;
    size_t i, n;
    fc_sb_init(&b);
    elements = json_obj_get(content, "elements");
    if (!elements || elements->type != JSON_ARRAY) return fc_sb_take(&b);
    n = json_len(elements);
    for (i = 0; i < n; i++) {
        const json_t *elem = json_get(elements, i);
        const char *etype;
        if (!elem || elem->type != JSON_OBJECT) continue;
        etype = fc_s(elem, "type");
        if (!strcmp(etype, "text_run") || !strcmp(etype, "docs_link")) {
            const json_t *box = json_obj_get(elem, etype);
            const json_t *val = box && box->type == JSON_OBJECT
                                    ? json_obj_get(box, !strcmp(etype, "text_run") ? "text" : "url")
                                    : NULL;
            char *sval = fc_py_str(val);
            fc_sb_add(&b, sval);
            free(sval);
        } else if (!strcmp(etype, "person")) {
            const json_t *person = json_obj_get(elem, "person");
            const json_t *uid = person && person->type == JSON_OBJECT ? json_obj_get(person, "user_id") : NULL;
            char *ustr;
            if (!person || person->type != JSON_OBJECT || !uid) {
                ustr = fc_dup(semantic ? "" : "unknown");
            } else {
                ustr = fc_py_str(uid);
            }
            if (semantic && self_open_id && self_open_id[0] && fc_same(ustr, self_open_id)) {
                free(ustr);
                continue;
            }
            fc_sb_ch(&b, '@');
            fc_sb_add(&b, ustr);
            free(ustr);
        }
    }
    {
        char *raw = fc_sb_take(&b);
        if (!semantic) return raw;
        {
            char *collapsed = fc_collapse_ws(raw);
            free(raw);
            return collapsed;
        }
    }
}

static char *fc_extract_reply_text(const json_t *reply, int semantic, const char *self_open_id) {
    const json_t *raw;
    json_t *content;
    int bad = 0;
    char *out;
    if (!reply || reply->type != JSON_OBJECT) return fc_dup("");
    raw = json_obj_get(reply, "content");
    if (!raw) return fc_dup(""); /* reply.get("content", {}) -> {} */
    content = fc_maybe_json_owned(raw, &bad);
    if (bad) return fc_py_str(raw); /* undecodable string: return it unchanged */
    if (!content) return fc_dup("");
    if (content->type != JSON_OBJECT) { /* non-dict content: seam, keep the raw value */
        out = fc_py_str(raw);
        json_free(content);
        return out;
    }
    out = fc_elements_text(content, semantic, self_open_id);
    json_free(content);
    return out;
}

/* _get_reply_user_id */
static char *fc_reply_user_id(const json_t *reply) {
    const json_t *uid = json_obj_get(reply, "user_id");
    if (!uid) return fc_dup("");
    if (uid->type == JSON_OBJECT) {
        const json_t *open_id = json_obj_get(uid, "open_id");
        const json_t *fallback = json_obj_get(uid, "user_id");
        if (fc_truthy(open_id)) return fc_py_str(open_id);
        return fc_py_str(fallback);
    }
    return fc_py_str(uid);
}

/* _reply_list_replies -> owned array */
static json_t *fc_reply_list_array(const json_t *whole_comment) {
    const json_t *rl = whole_comment && whole_comment->type == JSON_OBJECT
                           ? json_obj_get(whole_comment, "reply_list")
                           : NULL;
    json_t *parsed = NULL;
    json_t *out = json_array();
    const json_t *replies = NULL;
    if (rl && rl->type == JSON_STRING) {
        parsed = json_parse(rl->str_val ? rl->str_val : "", NULL);
        rl = parsed; /* undecodable -> fallback {} -> replies [] */
    }
    if (rl && rl->type == JSON_OBJECT) replies = json_obj_get(rl, "replies");
    if (replies && replies->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(replies); i++) json_append(out, json_copy(json_get(replies, i)));
    }
    if (parsed) json_free(parsed);
    return out;
}

/* ------------------------------------------------------------------ */
/* document links (_FEISHU_DOC_URL_RE)                                 */
/* ------------------------------------------------------------------ */

static const char *const FC_DOC_HOSTS[4] = {
    "feishu.cn", "larkoffice.com", "larksuite.com", "lark.suite.com",
};

static const char *const FC_DOC_TYPES[10] = {
    "wiki", "doc", "docx", "sheet", "sheets", "slides", "mindnote", "bitable", "base", "file",
};

static int fc_token_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-';
}

/* re.search of the doc-URL pattern: leftmost position, alternatives in the
 * same order the Python alternation tries them. */
static int fc_doc_url_match(const char *url, char **doc_type, char **token) {
    size_t i, hi, di;
    if (!url) return 0;
    for (i = 0; url[i]; i++) {
        for (hi = 0; hi < 4; hi++) {
            size_t hl = strlen(FC_DOC_HOSTS[hi]);
            size_t a, b;
            if (strncmp(url + i, FC_DOC_HOSTS[hi], hl) != 0) continue;
            a = i + hl;
            if (url[a] != '/') continue;
            a++;
            for (di = 0; di < 10; di++) {
                size_t tl = strlen(FC_DOC_TYPES[di]);
                size_t run, k;
                if (strncmp(url + a, FC_DOC_TYPES[di], tl) != 0) continue;
                b = a + tl;
                if (url[b] != '/') continue;
                b++;
                run = 0;
                while (url[b + run] && fc_token_char(url[b + run])) run++;
                if (run < 10) continue;
                k = run > 40 ? 40 : run;
                *doc_type = fc_prefix_bytes(url + a, tl);
                *token = fc_prefix_bytes(url + b, k);
                return 1;
            }
        }
    }
    return 0;
}

/* _extract_docs_links -> owned array of {url, doc_type, token} */
static json_t *fc_extract_links(const json_t *replies) {
    json_t *out = json_array();
    size_t i, n = (replies && replies->type == JSON_ARRAY) ? json_len(replies) : 0;
    for (i = 0; i < n; i++) {
        const json_t *reply = json_get(replies, i);
        const json_t *raw, *elements;
        json_t *content;
        int bad = 0;
        size_t j, m;
        if (!reply || reply->type != JSON_OBJECT) continue;
        raw = json_obj_get(reply, "content");
        content = fc_maybe_json_owned(raw, &bad);
        if (bad || !content) { if (content) json_free(content); continue; }
        if (content->type != JSON_OBJECT) { json_free(content); continue; }
        elements = json_obj_get(content, "elements");
        m = (elements && elements->type == JSON_ARRAY) ? json_len(elements) : 0;
        for (j = 0; j < m; j++) {
            const json_t *elem = json_get(elements, j);
            const char *etype;
            const json_t *u;
            const char *url = NULL;
            char *dtype = NULL, *tok = NULL;
            int dup = 0;
            size_t q;
            json_t *row;
            if (!elem || elem->type != JSON_OBJECT) continue;
            etype = fc_s(elem, "type");
            if (strcmp(etype, "docs_link") && strcmp(etype, "link")) continue;
            u = json_obj_get(elem, "docs_link");
            if (!fc_truthy(u)) u = json_obj_get(elem, "link");
            if (fc_truthy(u) && u->type == JSON_OBJECT) {
                const json_t *uv = json_obj_get(u, "url");
                if (uv && uv->type == JSON_STRING) url = uv->str_val;
            }
            if (!url || !fc_doc_url_match(url, &dtype, &tok)) {
                free(dtype);
                free(tok);
                continue;
            }
            for (q = 0; q < json_len(out); q++) {
                const json_t *prev = json_get(out, q);
                if (!strcmp(fc_s(prev, "token"), tok)) { dup = 1; break; }
            }
            if (dup) { free(dtype); free(tok); continue; }
            row = json_object();
            json_set(row, "url", json_string(url));
            json_set(row, "doc_type", json_string(dtype));
            json_set(row, "token", json_string(tok));
            json_append(out, row);
            free(dtype);
            free(tok);
        }
        json_free(content);
    }
    return out;
}

/* _format_referenced_docs */
static char *fc_format_referenced_docs(const json_t *links, const char *current_file_token) {
    size_t i, n = (links && links->type == JSON_ARRAY) ? json_len(links) : 0;
    fc_sb b;
    int first = 1;
    if (n == 0) return fc_dup("");
    fc_sb_init(&b);
    fc_line(&b, "", &first);
    fc_line(&b, "Referenced documents in comments:", &first);
    for (i = 0; i < n; i++) {
        const json_t *link = json_get(links, i);
        const json_t *rt = json_obj_get(link, "resolved_type");
        const json_t *rk = json_obj_get(link, "resolved_token");
        char *rtype = rt ? fc_py_str(rt) : fc_dup(fc_s(link, "doc_type"));
        char *rtok = rk ? fc_py_str(rk) : fc_dup(fc_s(link, "token"));
        char *url80 = fc_prefix_cps(fc_s(link, "url"), 80);
        fc_sb line;
        char *linestr;
        fc_sb_init(&line);
        fc_sb_add(&line, "- ");
        fc_sb_add(&line, rtype);
        fc_sb_ch(&line, ':');
        fc_sb_add(&line, rtok);
        if (current_file_token && current_file_token[0] && fc_same(rtok, current_file_token))
            fc_sb_add(&line, " (same as current document)");
        fc_sb_add(&line, " (");
        fc_sb_add(&line, url80);
        fc_sb_ch(&line, ')');
        linestr = fc_sb_take(&line);
        fc_line(&b, linestr, &first);
        free(linestr);
        free(rtype);
        free(rtok);
        free(url80);
    }
    return fc_sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* timeline                                                            */
/* ------------------------------------------------------------------ */

/* _timeline_entry -> owned {"user_id","text","is_self"} */
static json_t *fc_timeline_entry(const json_t *reply, const char *self_open_id) {
    char *uid = fc_reply_user_id(reply);
    char *text = fc_extract_reply_text(reply, 0, "");
    json_t *out = json_object();
    json_set(out, "user_id", json_string(uid));
    json_set(out, "text", json_string(text));
    json_set(out, "is_self",
             json_bool(self_open_id && self_open_id[0] && fc_same(uid, self_open_id)));
    free(uid);
    free(text);
    return out;
}

/* One empty timeline slot for a non-dict reply (seam shape). */
static json_t *fc_timeline_stub(void) {
    json_t *stub = json_object();
    json_set(stub, "user_id", json_string(""));
    json_set(stub, "text", json_string(""));
    json_set(stub, "is_self", json_bool(0));
    return stub;
}

/* _select_timeline */
static json_t *fc_select_timeline(const json_t *timeline, size_t limit, long long center,
                                  const long long *pinned, size_t npinned) {
    size_t n = (timeline && timeline->type == JSON_ARRAY) ? json_len(timeline) : 0;
    char *sel;
    size_t count = 0, i;
    long long lo, hi;
    json_t *out;
    if (n <= limit) return timeline ? json_copy(timeline) : json_array();
    sel = (char *)calloc(n, 1);
    if (!sel) return json_array();
    for (i = 0; i < npinned; i++) {
        long long p = pinned[i];
        if (p >= 0 && (size_t)p < n && !sel[p]) { sel[p] = 1; count++; }
    }
    if (center >= 0 && (size_t)center < n && !sel[center]) { sel[center] = 1; count++; }
    lo = center - 1;
    hi = center + 1;
    while (count < limit && (lo >= 0 || (hi >= 0 && (size_t)hi < n))) {
        if (lo >= 0 && count < limit && (size_t)lo < n && !sel[lo]) { sel[lo] = 1; count++; }
        if (count < limit && hi >= 0 && (size_t)hi < n && !sel[hi]) { sel[hi] = 1; count++; }
        lo -= 1;
        hi += 1;
    }
    out = json_array();
    if (count == 0) {
        for (i = n - limit; i < n; i++) json_append(out, json_copy(json_get(timeline, i)));
    } else {
        for (i = 0; i < n; i++)
            if (sel[i]) json_append(out, json_copy(json_get(timeline, i)));
    }
    free(sel);
    return out;
}

/* ------------------------------------------------------------------ */
/* prompt assembly                                                     */
/* ------------------------------------------------------------------ */

static char *fc_build_prompt(const json_t *intro, const char *doc_url, const char *file_type,
                             const char *file_token, const json_t *ids, const char *label,
                             const json_t *timeline, const json_t *selected,
                             const char *referenced_docs) {
    fc_sb b;
    int first = 1;
    size_t i;
    size_t nsel = (selected && selected->type == JSON_ARRAY) ? json_len(selected) : 0;
    size_t ntl = (timeline && timeline->type == JSON_ARRAY) ? json_len(timeline) : 0;
    fc_sb_init(&b);
    for (i = 0; intro && i < json_len(intro); i++) fc_line(&b, fc_vs(json_get(intro, i)), &first);
    fc_line(&b, FC_MENTION_NOTE, &first);
    {
        fc_sb t;
        char *s;
        fc_sb_init(&t);
        fc_sb_add(&t, "Document link: ");
        fc_sb_add(&t, doc_url);
        s = fc_sb_take(&t);
        fc_line(&b, s, &first);
        free(s);
        fc_line(&b, "Current commented document:", &first);
        fc_sb_init(&t);
        fc_sb_add(&t, "- file_type=");
        fc_sb_add(&t, file_type);
        s = fc_sb_take(&t);
        fc_line(&b, s, &first);
        free(s);
        fc_sb_init(&t);
        fc_sb_add(&t, "- file_token=");
        fc_sb_add(&t, file_token);
        s = fc_sb_take(&t);
        fc_line(&b, s, &first);
        free(s);
    }
    for (i = 0; ids && i < json_len(ids); i++)
        fc_line(&b, fc_vs(json_get(ids, i)), &first);
    fc_line(&b, "", &first);
    {
        fc_sb t;
        char *s;
        fc_sb_init(&t);
        fc_sb_add(&t, label);
        fc_sb_add(&t, " (");
        fc_sb_int(&t, (long long)nsel);
        fc_sb_ch(&t, '/');
        fc_sb_int(&t, (long long)ntl);
        fc_sb_add(&t, " entries):");
        s = fc_sb_take(&t);
        fc_line(&b, s, &first);
        free(s);
    }
    for (i = 0; i < nsel; i++) {
        const json_t *e = json_get(selected, i);
        fc_sb t;
        char *s, *trunc;
        trunc = fc_truncate(fc_s(e, "text"), FC_PROMPT_TEXT_LIMIT);
        fc_sb_init(&t);
        fc_sb_ch(&t, '[');
        fc_sb_add(&t, fc_s(e, "user_id"));
        fc_sb_add(&t, "] ");
        fc_sb_add(&t, trunc);
        if (json_get_bool(e, "is_self", 0)) fc_sb_add(&t, " <-- YOU");
        s = fc_sb_take(&t);
        fc_line(&b, s, &first);
        free(s);
        free(trunc);
    }
    if (referenced_docs && referenced_docs[0]) fc_line(&b, referenced_docs, &first);
    fc_line(&b, "", &first);
    fc_line(&b, FC_COMMON_INSTRUCTIONS, &first);
    return fc_sb_take(&b);
}

/* build_whole_comment_prompt from already-chosen values. */
static char *fc_assemble_whole(const char *doc_title, const char *doc_url, const char *file_type,
                               const char *file_token, const char *comment_text,
                               const json_t *timeline, long long current_index,
                               long long nearest_self_index, const char *referenced_docs) {
    json_t *intro = json_array();
    json_t *ids = json_array();
    json_t *selected;
    long long pin[1];
    char *t_text, *prompt, *s;
    fc_sb b;
    t_text = fc_truncate(comment_text, FC_PROMPT_TEXT_LIMIT);
    fc_sb_init(&b);
    fc_sb_add(&b, "The user added a comment in \"");
    fc_sb_add(&b, doc_title);
    fc_sb_add(&b, "\".");
    s = fc_sb_take(&b);
    json_append(intro, json_string(s));
    free(s);
    fc_sb_init(&b);
    fc_sb_add(&b, "Current user comment text: \"");
    fc_sb_add(&b, t_text);
    fc_sb_add(&b, "\"");
    s = fc_sb_take(&b);
    json_append(intro, json_string(s));
    free(s);
    json_append(intro, json_string("This is a whole-document comment."));
    pin[0] = nearest_self_index;
    selected = fc_select_timeline(timeline, FC_WHOLE_TL_LIMIT, current_index, pin, 1);
    prompt = fc_build_prompt(intro, doc_url, file_type, file_token, ids,
                             "Whole-document comment timeline", timeline, selected,
                             referenced_docs);
    json_free(intro);
    json_free(ids);
    json_free(selected);
    free(t_text);
    return prompt;
}

/* build_local_comment_prompt from already-chosen values. */
static char *fc_assemble_local(const char *doc_title, const char *doc_url, const char *file_type,
                               const char *file_token, const char *comment_id,
                               const char *quote_text, const char *root_text,
                               const char *target_text, const json_t *timeline,
                               long long target_index, const char *referenced_docs) {
    json_t *intro = json_array();
    json_t *ids = json_array();
    json_t *selected;
    long long pin[2];
    char *t_target, *t_root, *t_quote, *idline, *prompt, *s;
    size_t n = (timeline && timeline->type == JSON_ARRAY) ? json_len(timeline) : 0;
    fc_sb b;

    t_target = fc_truncate(target_text, FC_PROMPT_TEXT_LIMIT);
    t_root = fc_truncate(root_text, FC_PROMPT_TEXT_LIMIT);
    t_quote = fc_truncate(quote_text, 500);

    fc_sb_init(&b);
    fc_sb_add(&b, "The user added a reply in \"");
    fc_sb_add(&b, doc_title);
    fc_sb_add(&b, "\".");
    s = fc_sb_take(&b);
    json_append(intro, json_string(s));
    free(s);
    fc_sb_init(&b);
    fc_sb_add(&b, "Current user comment text: \"");
    fc_sb_add(&b, t_target);
    fc_sb_add(&b, "\"");
    s = fc_sb_take(&b);
    json_append(intro, json_string(s));
    free(s);
    fc_sb_init(&b);
    fc_sb_add(&b, "Original comment text: \"");
    fc_sb_add(&b, t_root);
    fc_sb_add(&b, "\"");
    s = fc_sb_take(&b);
    json_append(intro, json_string(s));
    free(s);
    fc_sb_init(&b);
    fc_sb_add(&b, "Quoted content: \"");
    fc_sb_add(&b, t_quote);
    fc_sb_add(&b, "\"");
    s = fc_sb_take(&b);
    json_append(intro, json_string(s));
    free(s);
    fc_sb_init(&b);
    fc_sb_add(&b, "- comment_id=");
    fc_sb_add(&b, comment_id);
    idline = fc_sb_take(&b);
    json_append(ids, json_string(idline));

    pin[0] = 0;
    pin[1] = (long long)n - 1;
    selected = fc_select_timeline(timeline, FC_LOCAL_TL_LIMIT, target_index, pin, 2);
    prompt = fc_build_prompt(intro, doc_url, file_type, file_token, ids,
                             "Current comment card timeline", timeline, selected, referenced_docs);
    json_free(intro);
    json_free(ids);
    json_free(selected);
    free(t_target);
    free(t_root);
    free(t_quote);
    free(idline);
    return prompt;
}

/* ------------------------------------------------------------------ */
/* p1: parse_drive_comment_event                                       */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p1_parse_event(const json_t *payload) {
    static const char *const EVT_KEYS[4] = {"event_id", "comment_id", "reply_id", "timestamp"};
    static const char *const META_KEYS[3] = {"file_token", "file_type", "notice_type"};
    const json_t *evt, *meta, *fu, *tu;
    json_t *out;
    size_t i;
    char *s;

    evt = payload && payload->type == JSON_OBJECT ? json_obj_get(payload, "event") : NULL;
    if (!evt || evt->type != JSON_OBJECT) return json_null();
    meta = json_obj_get(evt, "notice_meta");
    if (meta && meta->type != JSON_OBJECT) meta = NULL;

    out = json_object();
    for (i = 0; i < 4; i++) {
        s = fc_py_str_or_empty(json_obj_get(evt, EVT_KEYS[i]));
        json_set(out, EVT_KEYS[i], json_string(s));
        free(s);
    }
    for (i = 0; i < 3; i++) {
        s = fc_py_str_or_empty(meta ? json_obj_get(meta, META_KEYS[i]) : NULL);
        json_set(out, META_KEYS[i], json_string(s));
        free(s);
    }
    fu = meta ? json_obj_get(meta, "from_user_id") : NULL;
    if (fu && fu->type != JSON_OBJECT) fu = NULL;
    tu = meta ? json_obj_get(meta, "to_user_id") : NULL;
    if (tu && tu->type != JSON_OBJECT) tu = NULL;
    s = fc_py_str_or_empty(fu ? json_obj_get(fu, "open_id") : NULL);
    json_set(out, "from_open_id", json_string(s));
    free(s);
    s = fc_py_str_or_empty(tu ? json_obj_get(tu, "open_id") : NULL);
    json_set(out, "to_open_id", json_string(s));
    free(s);
    json_set(out, "is_mentioned", json_bool(fc_truthy(json_obj_get(evt, "is_mentioned"))));
    return out;
}

/* A canned lark response's data field (object only). */
static const json_t *fc_resp_data(const json_t *results, size_t idx) {
    const json_t *r = json_get(results, idx);
    const json_t *d;
    if (!r || r->type != JSON_OBJECT) return NULL;
    d = json_obj_get(r, "data");
    return d && d->type == JSON_OBJECT ? d : NULL;
}

static long long fc_resp_code(const json_t *results, size_t idx) {
    return (long long)fc_num_of(json_obj_get(json_get(results, idx), "code"), 0.0);
}

/* obj.get(key, <string default>) */
static json_t *fc_get_or(const json_t *obj, const char *key, const char *str_default) {
    json_t *v = json_copy(json_obj_get(obj, key));
    if (v) return v;
    return json_string(str_default);
}

/* ------------------------------------------------------------------ */
/* p1: query_document_meta                                             */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p1_meta_result(const json_t *req) {
    const json_t *results = json_obj_get(req, "results");
    const json_t *data, *metas;
    const char *file_token = fc_s(req, "file_token");
    const char *file_type = fc_s(req, "file_type");
    json_t *out = json_object();
    json_t *result = json_object();
    json_t *body = json_object();
    json_t *docs = json_array();
    json_t *reqdoc = json_object();
    long long code = fc_resp_code(results, 0);

    /* the request the call makes, whatever the answer */
    json_set(reqdoc, "doc_token", json_string(file_token));
    json_set(reqdoc, "doc_type", json_string(file_type));
    json_append(docs, reqdoc);
    json_set(body, "request_docs", docs);
    json_set(body, "with_url", json_bool(1));

    data = fc_resp_data(results, 0);
    if (code == 0) {
        metas = data ? json_obj_get(data, "metas") : NULL;
        if (metas && (fc_truthy(metas) || metas->type == JSON_OBJECT)) {
            /* metas[0] for a non-empty list; {} for any other truthy shape
             * (upstream's conditional prefers the empty dict there). */
            const json_t *meta = NULL;
            if (metas->type == JSON_ARRAY && json_len(metas) > 0) meta = json_get(metas, 0);
            if (!meta || meta->type != JSON_OBJECT) meta = NULL;
            json_set(result, "title", meta ? fc_get_or(meta, "title", "") : json_string(""));
            json_set(result, "url", meta ? fc_get_or(meta, "url", "") : json_string(""));
            json_set(result, "doc_type",
                     meta ? fc_get_or(meta, "doc_type", file_type) : json_string(file_type));
        }
    }

    json_set(out, "result", result);
    json_set(out, "method", json_string("POST"));
    json_set(out, "uri", json_string(FC_BATCH_META_URI));
    json_set(out, "paths", json_null());
    json_set(out, "queries", json_null());
    json_set(out, "body", body);
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: batch_query_comment                                             */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p1_comment_result(const json_t *req) {
    const json_t *results = json_obj_get(req, "results");
    const char *file_token = fc_s(req, "file_token");
    const char *file_type = fc_s(req, "file_type");
    const char *comment_id = fc_s(req, "comment_id");
    json_t *out = json_object();
    json_t *result = json_object();
    json_t *paths = json_object();
    json_t *queries = json_array();
    json_t *body = json_object();
    json_t *ids = json_array();
    const json_t *data = NULL;
    long long attempts = 0, i;
    int ok = 0;

    for (i = 0; i < FC_RETRY_LIMIT; i++) {
        if (!results || results->type != JSON_ARRAY || (size_t)i >= json_len(results)) break;
        attempts++;
        if (fc_resp_code(results, (size_t)i) == 0) {
            ok = 1;
            data = fc_resp_data(results, (size_t)i);
            break;
        }
    }
    if (ok && data) {
        const json_t *items = json_obj_get(data, "items");
        if (items && items->type == JSON_ARRAY && json_len(items) > 0) {
            json_free(result);
            result = json_copy(json_get(items, 0));
        }
    }

    {
        json_t *p1 = json_array(), *p2 = json_array();
        json_append(p1, json_string("file_type"));
        json_append(p1, json_string(file_type));
        json_append(p2, json_string("user_id_type"));
        json_append(p2, json_string("open_id"));
        json_append(queries, p1);
        json_append(queries, p2);
    }
    json_append(ids, json_string(comment_id));
    json_set(body, "comment_ids", ids);
    json_set(paths, "file_token", json_string(file_token));

    json_set(out, "result", result);
    json_set(out, "attempts", json_number((double)attempts));
    json_set(out, "method", json_string("POST"));
    json_set(out, "uri", json_string(FC_BATCH_COMMENT_URI));
    json_set(out, "paths", paths);
    json_set(out, "queries", queries);
    json_set(out, "body", body);
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: _list_all_pages                                                 */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p1_pages_result(const json_t *req) {
    const json_t *pages = json_obj_get(req, "pages");
    json_t *out = json_object();
    json_t *items_out = json_array();
    long long calls = 0;
    int fetch_ok = 1;
    size_t i;
    for (i = 0; i < FC_MAX_PAGES; i++) {
        const json_t *data, *items, *page_token;
        if (!pages || pages->type != JSON_ARRAY || i >= json_len(pages)) break;
        calls++;
        if (fc_resp_code(pages, i) != 0) { fetch_ok = 0; break; }
        data = fc_resp_data(pages, i);
        items = data ? json_obj_get(data, "items") : NULL;
        if (items && items->type == JSON_ARRAY) {
            size_t j;
            for (j = 0; j < json_len(items); j++)
                json_append(items_out, json_copy(json_get(items, j)));
        }
        page_token = (data && json_get_bool(data, "has_more", 0)) ? json_obj_get(data, "page_token") : NULL;
        if (!fc_truthy(page_token)) break;
    }
    json_set(out, "items", items_out);
    json_set(out, "fetch_ok", json_bool(fetch_ok));
    json_set(out, "calls", json_number((double)calls));
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: update_comment_reaction                                         */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p1_reaction_result(const json_t *req) {
    const char *action = fc_s(req, "action");
    const char *file_token = fc_s(req, "file_token");
    const char *file_type = fc_s(req, "file_type");
    const char *reply_id = fc_s(req, "reply_id");
    const json_t *rt = json_obj_get(req, "reaction_type");
    const char *reaction_type = (rt && rt->type == JSON_STRING) ? rt->str_val : "OK";
    long long code = (long long)fc_num_of(json_obj_get(req, "code"), 0.0);
    json_t *out = json_object();
    json_t *paths = json_object();
    json_t *queries = json_array();
    json_t *pair = json_array();
    json_t *body = json_object();

    json_set(paths, "file_token", json_string(file_token));
    json_append(pair, json_string("file_type"));
    json_append(pair, json_string(file_type));
    json_append(queries, pair);
    json_set(body, "action", json_string(action));
    json_set(body, "reply_id", json_string(reply_id));
    json_set(body, "reaction_type", json_string(reaction_type));

    json_set(out, "ok", json_bool(code == 0));
    json_set(out, "method", json_string("POST"));
    json_set(out, "uri", json_string(FC_REACTION_URI));
    json_set(out, "paths", paths);
    json_set(out, "queries", queries);
    json_set(out, "body", body);
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: reply_to_comment / add_whole_comment                            */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p1_reply_result(const json_t *req) {
    const char *file_token = fc_s(req, "file_token");
    const char *file_type = fc_s(req, "file_type");
    const char *comment_id = fc_s(req, "comment_id");
    char *text = fc_sanitize(fc_s(req, "text"));
    long long code = (long long)fc_num_of(json_obj_get(req, "code"), 0.0);
    json_t *out = json_object();
    json_t *paths = json_object();
    json_t *queries = json_array();
    json_t *pair = json_array();
    json_t *body = json_object();
    json_t *content = json_object();
    json_t *elements = json_array();
    json_t *elem = json_object();
    json_t *tr = json_object();

    json_set(tr, "text", json_string(text));
    json_set(elem, "type", json_string("text_run"));
    json_set(elem, "text_run", tr);
    json_append(elements, elem);
    json_set(content, "elements", elements);
    json_set(body, "content", content);

    json_set(paths, "file_token", json_string(file_token));
    json_set(paths, "comment_id", json_string(comment_id));
    json_append(pair, json_string("file_type"));
    json_append(pair, json_string(file_type));
    json_append(queries, pair);

    json_set(out, "ok", json_bool(code == 0));
    json_set(out, "code", json_number((double)code));
    json_set(out, "method", json_string("POST"));
    json_set(out, "uri", json_string(FC_REPLIES_URI));
    json_set(out, "paths", paths);
    json_set(out, "queries", queries);
    json_set(out, "body", body);
    free(text);
    return out;
}

json_t *w30_feishucomm_6493fb_p1_whole_comment_result(const json_t *req) {
    const char *file_token = fc_s(req, "file_token");
    const char *file_type = fc_s(req, "file_type");
    char *text = fc_sanitize(fc_s(req, "text"));
    long long code = (long long)fc_num_of(json_obj_get(req, "code"), 0.0);
    json_t *out = json_object();
    json_t *paths = json_object();
    json_t *body = json_object();
    json_t *elements = json_array();
    json_t *elem = json_object();

    json_set(elem, "type", json_string("text"));
    json_set(elem, "text", json_string(text));
    json_append(elements, elem);
    json_set(body, "file_type", json_string(file_type));
    json_set(body, "reply_elements", elements);
    json_set(paths, "file_token", json_string(file_token));

    json_set(out, "ok", json_bool(code == 0));
    json_set(out, "method", json_string("POST"));
    json_set(out, "uri", json_string(FC_ADD_COMMENT_URI));
    json_set(out, "paths", paths);
    json_set(out, "queries", json_null());
    json_set(out, "body", body);
    free(text);
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: deliver_comment_reply                                           */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p1_deliver_result(const json_t *req) {
    const json_t *reply_codes = json_obj_get(req, "reply_codes");
    const json_t *whole_oks = json_obj_get(req, "whole_ok");
    json_t *chunks = fc_chunk_text(fc_s(req, "text"), FC_REPLY_CHUNK_SIZE);
    json_t *out = json_object();
    json_t *steps = json_array();
    size_t rc = 0, wc = 0, i;
    int is_whole = json_get_bool(req, "is_whole", 0);
    int final_ok = 1;

    for (i = 0; i < json_len(chunks); i++) {
        const char *chunk = fc_vs(json_get(chunks, i));
        int ok = 1;
        long long code = 0;
        if (!is_whole) {
            const json_t *rcv = reply_codes ? json_get(reply_codes, rc) : NULL;
            rc++;
            code = (long long)fc_num_of(json_obj_get(rcv, "code"), 0.0);
            if (!json_obj_get(rcv, "code")) code = (long long)fc_num_of(rcv, 0.0);
            ok = code == 0;
            {
                json_t *step = json_object();
                json_set(step, "api", json_string("reply"));
                json_set(step, "text", json_string(chunk));
                json_set(step, "ok", json_bool(ok));
                json_set(step, "code", json_number((double)code));
                json_append(steps, step);
            }
            if (!ok && code == 1069302) is_whole = 1;
        }
        if (is_whole) {
            const json_t *wv = whole_oks ? json_get(whole_oks, wc) : NULL;
            wc++;
            ok = wv ? fc_truthy(wv) : 1;
            {
                json_t *step = json_object();
                json_set(step, "api", json_string("whole"));
                json_set(step, "text", json_string(chunk));
                json_set(step, "ok", json_bool(ok));
                json_append(steps, step);
            }
        }
        if (!ok) { final_ok = 0; break; }
    }
    json_set(out, "ok", json_bool(final_ok));
    json_set(out, "steps", steps);
    json_free(chunks);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: sanitize / chunk / truncate                                     */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p2_sanitize(const json_t *req) {
    char *s = fc_sanitize(fc_s(req, "text"));
    json_t *out = json_string(s);
    free(s);
    return out;
}

json_t *w30_feishucomm_6493fb_p2_chunk(const json_t *req) {
    const json_t *lim = json_obj_get(req, "limit");
    size_t limit = lim ? (size_t)fc_num_of(lim, (double)FC_REPLY_CHUNK_SIZE) : FC_REPLY_CHUNK_SIZE;
    return fc_chunk_text(fc_s(req, "text"), limit);
}

json_t *w30_feishucomm_6493fb_p2_truncate(const json_t *req) {
    const json_t *lim = json_obj_get(req, "limit");
    size_t limit = lim ? (size_t)fc_num_of(lim, (double)FC_PROMPT_TEXT_LIMIT) : FC_PROMPT_TEXT_LIMIT;
    char *s = fc_truncate(fc_s(req, "text"), limit);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: reply text / user / reply_list                                  */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p2_extract_text(const json_t *req) {
    const json_t *reply = json_obj_get(req, "reply");
    char *s = fc_extract_reply_text(reply, json_get_bool(req, "semantic", 0),
                                    fc_s(req, "self_open_id"));
    json_t *out = json_string(s);
    free(s);
    return out;
}

json_t *w30_feishucomm_6493fb_p2_reply_user(const json_t *req) {
    const json_t *reply = json_obj_get(req, "reply");
    char *s = (reply && reply->type == JSON_OBJECT) ? fc_reply_user_id(reply) : fc_dup("");
    json_t *out = json_string(s);
    free(s);
    return out;
}

json_t *w30_feishucomm_6493fb_p2_reply_list(const json_t *req) {
    return fc_reply_list_array(json_obj_get(req, "whole"));
}

/* ------------------------------------------------------------------ */
/* p2: document links                                                  */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p2_extract_links(const json_t *req) {
    return fc_extract_links(json_obj_get(req, "replies"));
}

json_t *w30_feishucomm_6493fb_p2_format_docs(const json_t *req) {
    char *s = fc_format_referenced_docs(json_obj_get(req, "links"), fc_s(req, "current"));
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: timeline                                                        */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p2_timeline(const json_t *req) {
    const json_t *replies = json_obj_get(req, "replies");
    const char *self_open_id = fc_s(req, "self_open_id");
    json_t *out = json_array();
    size_t i, n = (replies && replies->type == JSON_ARRAY) ? json_len(replies) : 0;
    for (i = 0; i < n; i++) {
        const json_t *r = json_get(replies, i);
        if (!r || r->type != JSON_OBJECT) { json_append(out, fc_timeline_stub()); continue; }
        json_append(out, fc_timeline_entry(r, self_open_id));
    }
    return out;
}

json_t *w30_feishucomm_6493fb_p2_select(const json_t *req) {
    const json_t *timeline = json_obj_get(req, "timeline");
    const json_t *pinned = json_obj_get(req, "pinned");
    size_t limit = (size_t)fc_num_of(json_obj_get(req, "limit"), 0.0);
    long long center = (long long)fc_num_of(json_obj_get(req, "center"), -1.0);
    long long *pin = NULL;
    size_t npin = 0, i;
    json_t *out;
    if (pinned && pinned->type == JSON_ARRAY && json_len(pinned) > 0) {
        npin = json_len(pinned);
        pin = (long long *)calloc(npin, sizeof(long long));
        if (pin)
            for (i = 0; i < npin; i++)
                pin[i] = (long long)fc_num_of(json_get(pinned, i), 0.0);
    }
    out = fc_select_timeline(timeline, limit, center, pin, npin);
    free(pin);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: prompt builders                                                 */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p2_build_local(const json_t *req) {
    char *prompt = fc_assemble_local(fc_s(req, "doc_title"), fc_s(req, "doc_url"),
                                     fc_s(req, "file_type"), fc_s(req, "file_token"),
                                     fc_s(req, "comment_id"), fc_s(req, "quote_text"),
                                     fc_s(req, "root_comment_text"),
                                     fc_s(req, "target_reply_text"),
                                     json_obj_get(req, "timeline"),
                                     (long long)fc_num_of(json_obj_get(req, "target_index"), -1.0),
                                     fc_s(req, "referenced_docs"));
    json_t *out = json_string(prompt);
    free(prompt);
    return out;
}

json_t *w30_feishucomm_6493fb_p2_build_whole(const json_t *req) {
    char *prompt = fc_assemble_whole(fc_s(req, "doc_title"), fc_s(req, "doc_url"),
                                     fc_s(req, "file_type"), fc_s(req, "file_token"),
                                     fc_s(req, "comment_text"), json_obj_get(req, "timeline"),
                                     (long long)fc_num_of(json_obj_get(req, "current_index"), -1.0),
                                     (long long)fc_num_of(json_obj_get(req, "nearest_self_index"), -1.0),
                                     fc_s(req, "referenced_docs"));
    json_t *out = json_string(prompt);
    free(prompt);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: whole/local prompt from already-fetched replies                 */
/* ------------------------------------------------------------------ */

json_t *w30_feishucomm_6493fb_p2_whole_prompt(const json_t *req) {
    const json_t *doc = json_obj_get(req, "doc");
    const json_t *whole_comments = json_obj_get(req, "whole_comments");
    const char *from_open_id = fc_s(req, "from_open_id");
    const char *self_open_id = fc_s(doc, "self_open_id");
    const char *file_token = fc_s(doc, "file_token");
    json_t *raw = json_array();
    json_t *timeline, *links;
    char *current_text, *referenced, *prompt;
    long long current_index, nearest_self;
    size_t i;
    size_t n_wc = (whole_comments && whole_comments->type == JSON_ARRAY) ? json_len(whole_comments) : 0;

    for (i = 0; i < n_wc; i++) {
        json_t *replies = fc_reply_list_array(json_get(whole_comments, i));
        size_t j;
        for (j = 0; j < json_len(replies); j++) json_append(raw, json_copy(json_get(replies, j)));
        json_free(replies);
    }
    timeline = json_array();
    for (i = 0; i < json_len(raw); i++) {
        const json_t *r = json_get(raw, i);
        if (!r || r->type != JSON_OBJECT) { json_append(timeline, fc_timeline_stub()); continue; }
        json_append(timeline, fc_timeline_entry(r, self_open_id));
    }

    /* current_text / current_index / nearest_self_index */
    current_text = fc_dup("");
    current_index = -1;
    nearest_self = -1;
    for (i = 0; i < json_len(timeline); i++) {
        const json_t *entry = json_get(timeline, i);
        if (from_open_id[0] && fc_same(fc_s(entry, "user_id"), from_open_id)) {
            free(current_text);
            current_text = fc_extract_reply_text(json_get(raw, i), 1, self_open_id);
            current_index = (long long)i;
        }
        if (json_get_bool(entry, "is_self", 0)) nearest_self = (long long)i;
    }
    if (!current_text[0]) {
        for (i = json_len(timeline); i > 0; i--) {
            const json_t *entry = json_get(timeline, i - 1);
            if (!json_get_bool(entry, "is_self", 0)) {
                free(current_text);
                current_text = fc_dup(fc_s(entry, "text"));
                current_index = (long long)(i - 1);
                break;
            }
        }
    }

    links = fc_extract_links(raw);
    referenced = fc_format_referenced_docs(links, file_token);
    prompt = fc_assemble_whole(fc_s(doc, "doc_title"), fc_s(doc, "doc_url"),
                               fc_s(doc, "file_type"), file_token, current_text, timeline,
                               current_index, nearest_self, referenced);
    {
        json_t *out = json_string(prompt);
        free(prompt);
        free(current_text);
        free(referenced);
        json_free(links);
        json_free(timeline);
        json_free(raw);
        return out;
    }
}

json_t *w30_feishucomm_6493fb_p2_local_prompt(const json_t *req) {
    const json_t *doc = json_obj_get(req, "doc");
    const json_t *replies_in = json_obj_get(req, "replies");
    const char *reply_id = fc_s(req, "reply_id");
    const char *from_open_id = fc_s(req, "from_open_id");
    const char *quote_text = fc_s(req, "quote_text");
    const char *self_open_id = fc_s(doc, "self_open_id");
    const char *file_token = fc_s(doc, "file_token");
    const char *comment_id = fc_s(req, "comment_id");
    json_t *raw = (replies_in && replies_in->type == JSON_ARRAY) ? json_copy(replies_in) : json_array();
    json_t *timeline, *links;
    char *root_text, *target_text, *referenced, *prompt;
    long long target_index;
    size_t i, n = json_len(raw);

    timeline = json_array();
    for (i = 0; i < n; i++) {
        const json_t *r = json_get(raw, i);
        if (!r || r->type != JSON_OBJECT) { json_append(timeline, fc_timeline_stub()); continue; }
        json_append(timeline, fc_timeline_entry(r, self_open_id));
    }
    root_text = n ? fc_extract_reply_text(json_get(raw, 0), 1, self_open_id) : fc_dup("");

    target_text = fc_dup("");
    target_index = -1;
    if (reply_id[0]) {
        for (i = 0; i < n; i++) {
            const json_t *r = json_get(raw, i);
            if (!fc_same(fc_s(r, "reply_id"), reply_id)) continue;
            free(target_text);
            target_text = fc_extract_reply_text(r, 1, self_open_id);
            target_index = (long long)i;
        }
    }
    if (!target_text[0]) {
        for (i = n; i > 0; i--) {
            const json_t *entry = json_get(timeline, i - 1);
            if (from_open_id[0] && fc_same(fc_s(entry, "user_id"), from_open_id)) {
                free(target_text);
                target_text = fc_dup(fc_s(entry, "text"));
                target_index = (long long)(i - 1);
                break;
            }
        }
    }

    links = fc_extract_links(raw);
    referenced = fc_format_referenced_docs(links, file_token);
    prompt = fc_assemble_local(fc_s(doc, "doc_title"), fc_s(doc, "doc_url"),
                               fc_s(doc, "file_type"), file_token, comment_id, quote_text,
                               root_text, target_text, timeline, target_index, referenced);
    {
        json_t *out = json_string(prompt);
        free(prompt);
        free(root_text);
        free(target_text);
        free(referenced);
        json_free(links);
        json_free(timeline);
        json_free(raw);
        return out;
    }
}
