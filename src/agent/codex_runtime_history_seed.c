/*
 * codex_runtime_history_seed.c - C11 port of upstream
 * agent/codex_runtime_history_seed.py (gate w25_codexrunti_62951b).
 *
 * PoP: w25_codexrunti_62951b_text_of @ agent/codex_runtime_history_seed.py:_text_of
 * PoP: w25_codexrunti_62951b_render_row @ agent/codex_runtime_history_seed.py:_render_row
 * PoP: w25_codexrunti_62951b_render_seed @ agent/codex_runtime_history_seed.py:render_history_seed
 * PoP: w25_codexrunti_62951b_constants @ agent/codex_runtime_history_seed.py:<module constants>
 *
 * The codex app-server (thread/start, thread/resume, the transport) is a seam.
 * What is ported is the part that is a pure function of values: which rows of a
 * stored transcript survive (the trailing user row is the turn being submitted),
 * how each one renders, and how the tail cap keeps the most recent turns.
 * len()/slicing count code points, as Python does.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_codexrunti_62951b.h"

/* ---------------------------------------------------------------- buffers */

typedef struct {
    char  *p;
    size_t n, cap;
} cs_buf;

static void cs_reserve(cs_buf *b, size_t extra) {
    size_t need = b->n + extra + 1;
    size_t cap;
    char *np;
    if (need <= b->cap) return;
    cap = b->cap ? b->cap : 128;
    while (cap < need) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) return;
    b->p = np;
    b->cap = cap;
    if (b->n < cap) b->p[b->n] = '\0';
}

static void cs_putn(cs_buf *b, const char *s, size_t n) {
    if (!s || !n) return;
    cs_reserve(b, n);
    if (b->cap < b->n + n + 1) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void cs_puts(cs_buf *b, const char *s) { if (s) cs_putn(b, s, strlen(s)); }

static const char *cs_cstr(cs_buf *b) { return b->p ? b->p : ""; }

static void cs_free(cs_buf *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* ------------------------------------------------------------- utf8 count */

static unsigned int cs_decode(const unsigned char *s, size_t len, size_t i, size_t *used) {
    unsigned int c = s[i];
    if (c < 0x80) { *used = 1; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < len) { *used = 2; return ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); }
    if ((c & 0xF0) == 0xE0 && i + 2 < len) {
        *used = 3;
        return ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu);
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < len) {
        *used = 4;
        return ((c & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12) | ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu);
    }
    *used = 1;
    return c;
}

static size_t cs_cp_len_n(const char *s, size_t len) {
    size_t i = 0, n = 0;
    while (i < len) { size_t u = 1; (void)cs_decode((const unsigned char *)s, len, i, &u); i += u ? u : 1; n++; }
    return n;
}

static size_t cs_cp_len(const char *s) { return s ? cs_cp_len_n(s, strlen(s)) : 0; }

/* Byte offset of code point ``cps`` (or the byte length when it runs out). */
static size_t cs_prefix_bytes(const char *s, size_t cps) {
    size_t i = 0, n = 0, len = s ? strlen(s) : 0;
    while (i < len && n < cps) { size_t u = 1; (void)cs_decode((const unsigned char *)s, len, i, &u); i += u ? u : 1; n++; }
    return i;
}

/* Byte offset where the last ``cps`` code points start. */
static size_t cs_suffix_start(const char *s, size_t cps) {
    size_t len = s ? strlen(s) : 0;
    size_t total = cs_cp_len_n(s ? s : "", len);
    if (cps >= total) return 0;
    return cs_prefix_bytes(s, total - cps);
}

static int cs_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int cs_truthy_str(const char *s) { return s && s[0] != '\0'; }

static int cs_streq(const json_t *v, const char *lit) {
    return v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, lit) == 0;
}

/* ---------------------------------------------------------------- _text_of */

/* Append one content part as Python would: a str is itself, a dict is its
 * "text" member ("" when absent), anything else is skipped. */
static void cs_text_of_into(const json_t *content, cs_buf *out) {
    size_t i, n, added = 0;
    if (!content) return;
    if (content->type == JSON_STRING) { cs_puts(out, content->str_val ? content->str_val : ""); return; }
    if (content->type != JSON_ARRAY) return;

    n = json_len(content);
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(content, i);
        const char *part = NULL;
        if (!p) continue;
        if (p->type == JSON_STRING) {
            part = p->str_val ? p->str_val : "";
        } else if (p->type == JSON_OBJECT) {
            const json_t *t = json_obj_get(p, "text");
            part = (t && t->type == JSON_STRING) ? (t->str_val ? t->str_val : "") : "";
        } else {
            continue;                     /* isinstance(p, (str, dict)) gate */
        }
        if (!cs_truthy_str(part)) continue;   /* "\\n".join(p for p in parts if p) */
        if (added) cs_puts(out, "\n");
        cs_puts(out, part);
        added++;
    }
}

json_t *w25_codexrunti_62951b_text_of(const json_t *content) {
    cs_buf b = {0};
    json_t *out;
    cs_text_of_into(content, &b);
    out = json_string(cs_cstr(&b));
    cs_free(&b);
    return out;
}

/* ------------------------------------------------------------- _render_row */

/* text.strip() into a fresh buffer. */
static char *cs_strip(const char *s) {
    size_t n = s ? strlen(s) : 0, start = 0;
    char *out;
    while (start < n && cs_space(s[start])) start++;
    while (n > start && cs_space(s[n - 1])) n--;
    out = (char *)malloc(n - start + 1);
    if (!out) return NULL;
    memcpy(out, s + start, n - start);
    out[n - start] = '\0';
    return out;
}

json_t *w25_codexrunti_62951b_render_row(const json_t *msg) {
    const json_t *role, *calls;
    char *text = NULL;
    cs_buf out = {0};
    json_t *res;

    if (!msg || msg->type != JSON_OBJECT) return json_string("");
    role = json_obj_get(msg, "role");
    {
        cs_buf tb = {0};
        cs_text_of_into(json_obj_get(msg, "content"), &tb);
        text = cs_strip(cs_cstr(&tb));
        cs_free(&tb);
    }
    if (!text) return json_string("");

    if (cs_streq(role, "user")) {
        if (text[0]) { cs_puts(&out, "[USER]\n"); cs_puts(&out, text); }
    } else if (cs_streq(role, "assistant")) {
        int wrote = 0;
        if (text[0]) { cs_puts(&out, "[ASSISTANT]\n"); cs_puts(&out, text); wrote = 1; }
        calls = json_obj_get(msg, "tool_calls");
        if (calls && calls->type == JSON_ARRAY) {
            size_t i, n = json_len(calls), dicts = 0, named = 0;
            for (i = 0; i < n; i++) {
                const json_t *c = json_get(calls, i);
                if (!c || c->type != JSON_OBJECT) continue;
                dicts++;   /* len(calls) after the isinstance-gate */
            }
            if (dicts > 0) {
                if (wrote) cs_puts(&out, "\n");
                cs_puts(&out, "[ASSISTANT called tools: ");
                for (i = 0; i < n; i++) {
                    const json_t *c = json_get(calls, i);
                    const json_t *fn, *name;
                    if (!c || c->type != JSON_OBJECT) continue;
                    fn = json_obj_get(c, "function");
                    name = (fn && fn->type == JSON_OBJECT) ? json_obj_get(fn, "name") : NULL;
                    if (!name || !cs_truthy_str(name->str_val)) continue;
                    if (named) cs_puts(&out, ", ");
                    cs_puts(&out, name->str_val);
                    named++;
                }
                cs_puts(&out, "]");
            }
        }
    } else if (cs_streq(role, "tool")) {
        if (cs_cp_len(text) > W25_CODEXRUNTI_62951B_TOOL_RESULT_PREVIEW_CHARS) {
            size_t bytes = cs_prefix_bytes(text, W25_CODEXRUNTI_62951B_TOOL_RESULT_PREVIEW_CHARS);
            cs_puts(&out, "[TOOL RESULT]\n");
            cs_putn(&out, text, bytes);
            cs_puts(&out, " \xe2\x80\xa6");
        } else if (text[0]) {
            cs_puts(&out, "[TOOL RESULT]\n");
            cs_puts(&out, text);
        }
    }

    res = json_string(cs_cstr(&out));
    cs_free(&out);
    free(text);
    return res;
}

/* ----------------------------------------------------------- render_history */

json_t *w25_codexrunti_62951b_render_seed(const json_t *req) {
    const json_t *messages;
    size_t i, n, rows;
    cs_buf body = {0}, out = {0};
    json_t *res = NULL;

    messages = req && req->type == JSON_OBJECT ? json_obj_get(req, "messages") : NULL;
    if (!messages || messages->type != JSON_ARRAY) return json_string("");

    n = json_len(messages);
    rows = n;
    if (rows > 0) {
        const json_t *last = json_get(messages, rows - 1);
        if (last && last->type == JSON_OBJECT && cs_streq(json_obj_get(last, "role"), "user")) rows--;
    }

    {
        size_t rendered = 0;
        for (i = 0; i < rows; i++) {
            const json_t *m = json_get(messages, i);
            json_t *line;
            const char *txt;
            if (!m || m->type != JSON_OBJECT) continue;
            line = w25_codexrunti_62951b_render_row(m);
            txt = line && line->type == JSON_STRING ? (line->str_val ? line->str_val : "") : "";
            if (!cs_truthy_str(txt)) { json_free(line); continue; }
            if (rendered) cs_puts(&body, "\n\n");
            cs_puts(&body, txt);
            rendered++;
            json_free(line);
        }
        if (!rendered) return json_string("");
    }

    {
        const char *b = cs_cstr(&body);
        size_t total = cs_cp_len(b);
        if (total > (size_t)W25_CODEXRUNTI_62951B_MAX_HISTORY_SEED_CHARS) {
            size_t start = cs_suffix_start(b, W25_CODEXRUNTI_62951B_MAX_HISTORY_SEED_CHARS);
            cs_buf tail = {0};
            cs_puts(&tail, "[\xe2\x80\xa6 earlier turns omitted \xe2\x80\xa6]\n\n");
            cs_puts(&tail, b + start);
            cs_free(&body);
            body = tail;
        }
    }

    cs_puts(&out, W25_CODEXRUNTI_62951B_HEADER);
    cs_puts(&out, "\n\n");
    cs_puts(&out, cs_cstr(&body));
    res = json_string(cs_cstr(&out));
    cs_free(&body);
    cs_free(&out);
    return res;
}

/* -------------------------------------------------------------- constants */

json_t *w25_codexrunti_62951b_constants(void) {
    json_t *o = json_object();
    json_set(o, "max_history_seed_chars",
             json_number((double)W25_CODEXRUNTI_62951B_MAX_HISTORY_SEED_CHARS));
    json_set(o, "tool_result_preview_chars",
             json_number((double)W25_CODEXRUNTI_62951B_TOOL_RESULT_PREVIEW_CHARS));
    json_set(o, "header", json_string(W25_CODEXRUNTI_62951B_HEADER));
    return o;
}
