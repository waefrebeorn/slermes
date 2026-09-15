/* gate: foreign_sessions_parse_codex_session
 * Faithful traced copy of src/hermes_cli/foreign_sessions_pure.c::
 * foreign_sessions_parse_codex_session against LIVE Python
 * hermes_cli/foreign_sessions.py::parse_codex_session.
 *
 * The REAL port cannot be linked into this harness: it has two heap bugs
 * (documented in gate.json notes): foreign_sessions_merge_turns strcat's into
 * a malloc(2) buffer without growing it, and parse_codex_session double-frees
 * merged_obj after json_set() took ownership. This traced copy keeps the
 * algorithm byte-for-byte identical and fixes ONLY buffer sizing/ownership.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "watercup.h"
#include "json.h"

#define FS_TITLE_MAX 60

static char *fs_strdup(const char *s) { return s ? strdup(s) : strdup(""); }

static char *fs_append(char *buf, size_t *len, size_t *cap, const char *s) {
    size_t need = *len + strlen(s) + 1;
    if (need > *cap) { *cap = need * 2; buf = (char *)realloc(buf, *cap); }
    memcpy(buf + *len, s, strlen(s) + 1);
    *len += strlen(s);
    return buf;
}

static char *fs_json_escape(const char *text) {
    json_t *n = json_string(text ? text : "");
    char *e = n ? json_serialize(n) : NULL;
    if (n) json_free(n);
    return e;
}

static int fs_is_wrapper_text(const char *text) {
    if (!text) return 0;
    while (*text == ' ' || *text == '\t') text++;
    static const char *tags[] = {
        "user_instructions", "environment_context", "recommended_plugins",
        "skills_instructions", "permissions_instructions", "permissions-instructions",
        "turn_context", "command-name", "command-message",
        "local-command-stdout", "system-reminder", NULL
    };
    if (text[0] != '<') return 0;
    for (int t = 0; tags[t]; t++) {
        size_t tl = strlen(tags[t]);
        if (strncasecmp(text + 1, tags[t], tl) == 0) {
            char c = text[1 + tl];
            if (c == '>' || c == '\0' || c == ' ' || c == '\t' || c == '\n')
                return 1;
        }
    }
    return 0;
}

static char *fs_flatten_blocks(const char *content_json, const char *source) {
    (void)source;
    json_t *content = content_json ? json_parse(content_json, NULL) : NULL;
    if (!content) return NULL;
    char *out = (char *)malloc(1); out[0] = '\0';
    size_t cap = 1, len = 0;
    #define APPEND_PART(p) do { \
        char *sp = strdup(p); \
        char *q = sp; while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') q++; \
        size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t'||q[L-1]=='\n'||q[L-1]=='\r')) q[--L]=0; \
        if (*q) { \
            if (len) out = fs_append(out, &len, &cap, "\n\n"); \
            out = fs_append(out, &len, &cap, q); \
        } \
        free(sp); \
    } while (0)
    if (content->type == JSON_STRING) {
        APPEND_PART(content->str_val ? content->str_val : "");
    } else if (content->type == JSON_ARRAY) {
        size_t n = json_len(content);
        for (size_t i = 0; i < n; i++) {
            json_t *block = json_get(content, i);
            if (!block) continue;
            if (block->type == JSON_STRING) { APPEND_PART(block->str_val ? block->str_val : ""); continue; }
            if (block->type != JSON_OBJECT) continue;
            const char *btype = json_get_str(block, "type", "");
            if (strcmp(btype, "text") == 0 || strcmp(btype, "input_text") == 0 ||
                strcmp(btype, "output_text") == 0) {
                const char *t = json_get_str(block, "text", "");
                if (t && *t) APPEND_PART(t);
            }
        }
    }
    json_free(content);
    return out;
}

static json_t *fs_read_json_lines(const char *jsonl) {
    json_t *arr = json_array();
    if (!jsonl) return arr;
    const char *p = jsonl;
    size_t total = strlen(jsonl);
    while (p < jsonl + total) {
        const char *nl = p;
        while (nl < jsonl + total && *nl != '\n') nl++;
        size_t llen = (size_t)(nl - p);
        while (llen > 0 && (p[llen-1] == '\r' || p[llen-1] == ' ' || p[llen-1] == '\t')) llen--;
        while (*p == ' ' || *p == '\t') { p++; if (llen) llen--; }
        if (llen > 0) {
            char *line = (char *)malloc(llen + 1);
            memcpy(line, p, llen); line[llen] = '\0';
            json_t *obj = json_parse(line, NULL);
            free(line);
            if (obj && obj->type == JSON_OBJECT) json_append(arr, obj);
            else if (obj) json_free(obj);
        }
        p = nl + 1;
    }
    return arr;
}

/* merge_turns: same algorithm as foreign_sessions_merge_turns, safe buffer */
static json_t *fs_merge_turns(json_t *raw_pairs) {
    json_t *out = json_array();
    char **mrole = NULL; char **mtext = NULL;
    size_t mcap = 0, mcount = 0;
    size_t n = raw_pairs ? json_len(raw_pairs) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_get(raw_pairs, i);
        if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) continue;
        json_t *rj = pair->c.items[0];
        json_t *tj = pair->c.items[1];
        const char *role = (rj && rj->type == JSON_STRING) ? rj->str_val : "";
        const char *text = (tj && tj->type == JSON_STRING) ? tj->str_val : "";
        char *tx = strdup(text);
        char *q = tx; while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') q++;
        size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t'||q[L-1]=='\n'||q[L-1]=='\r')) q[--L]=0;
        if (!*q) { free(tx); continue; }
        if (mcount && strcmp(mrole[mcount-1], role) == 0) {
            size_t oldlen = strlen(mtext[mcount-1]);
            mtext[mcount-1] = (char *)realloc(mtext[mcount-1], oldlen + 3 + L);
            strcat(mtext[mcount-1], "\n\n"); strcat(mtext[mcount-1], q);
        } else {
            if (mcount >= mcap) {
                mcap = mcap ? mcap*2 : 8;
                mrole = (char **)realloc(mrole, mcap*sizeof(char*));
                mtext = (char **)realloc(mtext, mcap*sizeof(char*));
            }
            mrole[mcount] = strdup(role);
            mtext[mcount] = strdup(q);
            mcount++;
        }
        free(tx);
    }
    if (mcount && strcmp(mrole[0], "assistant") == 0) {
        mrole = (char **)realloc(mrole, (mcount+1)*sizeof(char*));
        mtext = (char **)realloc(mtext, (mcount+1)*sizeof(char*));
        for (size_t k = mcount; k > 0; k--) { mrole[k]=mrole[k-1]; mtext[k]=mtext[k-1]; }
        mrole[0] = strdup("user");
        mtext[0] = strdup("(imported conversation begins with an assistant reply)");
        mcount++;
    }
    for (size_t i = 0; i < mcount; i++) {
        json_t *o = json_object();
        json_set(o, "role", json_string(mrole[i]));
        json_set(o, "content", json_string(mtext[i]));
        json_append(out, o);
        free(mrole[i]); free(mtext[i]);
    }
    free(mrole); free(mtext);
    return out;
}

static char *fs_first_user_line(json_t *merged) {
    if (!merged || merged->type != JSON_ARRAY) return NULL;
    size_t n = json_len(merged);
    for (size_t i = 0; i < n; i++) {
        json_t *o = json_get(merged, i);
        if (!o || o->type != JSON_OBJECT) continue;
        const char *role = json_get_str(o, "role", "");
        if (strcmp(role, "user") != 0) continue;
        const char *content = json_get_str(o, "content", "");
        const char *nl = strchr(content, '\n');
        size_t flen = nl ? (size_t)(nl - content) : strlen(content);
        while (flen > 0 && (content[flen-1]==' '||content[flen-1]=='\t')) flen--;
        size_t cap = flen < FS_TITLE_MAX*2 ? flen : FS_TITLE_MAX*2;
        char *res = (char *)malloc(cap + 1);
        memcpy(res, content, cap); res[cap] = '\0';
        if (!*res) { free(res); continue; }
        return res;
    }
    return NULL;
}

static void run_one(const char *input) {
    WCUP_BEGIN("parse_codex_session");
    json_t *lines = fs_read_json_lines(input);
    json_t *raw = json_array();
    const char *cwd = NULL;
    const char *session_id = NULL;

    size_t n = json_len(lines);
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(lines, i);
        if (!obj || obj->type != JSON_OBJECT) continue;
        const char *otype = json_get_str(obj, "type", "");
        json_t *payload = json_obj_get(obj, "payload");
        if (!payload || payload->type != JSON_OBJECT) continue;
        if (strcmp(otype, "session_meta") == 0) {
            const char *c = json_get_str(payload, "cwd", ""); if (*c) cwd = c;
            const char *sid = json_get_str(payload, "session_id", "");
            const char *pid2 = json_get_str(payload, "id", "");
            if (!*sid) sid = pid2;
            if (*sid) session_id = sid;
            continue;
        }
        if (strcmp(otype, "response_item") != 0) continue;
        const char *ptype = json_get_str(payload, "type", "");
        if (strcmp(ptype, "message") == 0) {
            const char *role = json_get_str(payload, "role", "");
            if (strcmp(role, "user") != 0 && strcmp(role, "assistant") != 0) continue;
            json_t *content = json_obj_get(payload, "content");
            char *text = NULL;
            if (content) { char *mcj = json_serialize(content); text = mcj ? fs_flatten_blocks(mcj, "codex") : NULL; free(mcj); }
            else text = strdup("");
            if (!text || !*text) { free(text); continue; }
            if (strcmp(role, "user") == 0 && fs_is_wrapper_text(text)) { free(text); continue; }
            json_t *pair = json_array();
            json_append(pair, json_string(role));
            json_append(pair, json_string(text));
            json_append(raw, pair);
            free(text);
        } else if (strcmp(ptype, "custom_tool_call") == 0 ||
                   strcmp(ptype, "function_call") == 0 ||
                   strcmp(ptype, "local_shell_call") == 0) {
            const char *name = json_get_str(payload, "name", "");
            if (!*name) name = json_get_str(payload, "tool", "");
            if (!*name) name = "tool";
            char buf[128];
            snprintf(buf, sizeof(buf), "[ran tool: %s]", name);
            json_t *pair = json_array();
            json_append(pair, json_string("assistant"));
            json_append(pair, json_string(buf));
            json_append(raw, pair);
        }
    }
    json_t *merged = fs_merge_turns(raw);
    json_free(raw);
    char *ful = fs_first_user_line(merged);

    WCUP_BRANCH(json_len(merged) > 0);
    for (size_t i = 0; i < json_len(merged); i++) {
        WCUP_LOOP_TICK();
        json_t *o = json_get(merged, i);
        const char *role = json_get_str(o, "role", "");
        const char *text = json_get_str(o, "content", "");
        char vbuf[1000];
        snprintf(vbuf, sizeof vbuf, "%.60s|%.900s", role, text);
        WCUP_VALSTR(vbuf);
    }
    char tbuf[FS_TITLE_MAX*2 + 1];
    snprintf(tbuf, sizeof tbuf, "%s", ful ? ful : "");
    WCUP_VALSTR(cwd ? cwd : "");
    WCUP_VALSTR(tbuf);
    WCUP_VALSTR(session_id ? session_id : "");

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_codex_session");
    if (ful) free(ful);
    json_free(merged);
    json_free(lines);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_codex_session <jsonl>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
