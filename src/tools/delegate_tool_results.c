/*
 * delegate_tool_results.c - C home for upstream Hermes tools/delegate_tool_results.py.
 * PoP: w15_delegateto_c3b7d4_stringify @ tools/delegate_tool_results.py:_stringify_tool_content
 * PoP: w15_delegateto_c3b7d4_looks_error @ tools/delegate_tool_results.py:_looks_like_error_output
 *
 * Observability helpers for child tool results. The caller supplies already-decoded
 * JSON content; only deterministic stringification and the conservative error
 * detector live here. Redaction, budgets and hooks stay in Python.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_delegateto_c3b7d4.h"

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmti(long long v) { char b[48]; snprintf(b, sizeof(b), "%lld", v); return dupstr(b); }
static char *fmtg(double v) { char b[64]; snprintf(b, sizeof(b), "%g", v); return dupstr(b); }

typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, char c) {
    if (b->n + 1 >= b->cap) { size_t nc = b->cap ? b->cap * 2 : 64; char *np = (char *)realloc(b->p, nc); if (!np) return; b->p = np; b->cap = nc; }
    b->p[b->n++] = c;
}
static void bputs(buf_t *b, const char *s) { for (const char *q = s; *q; q++) bput(b, *q); }
static char *bdone(buf_t *b) { if (!b->p) b->p = (char *)calloc(1, 1); if (b->n) b->p[b->n] = 0; return b->p; }

static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL)   return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val ? fmti((long long)v->num_val) : fmtg(v->num_val);
    char *s = json_dumps(v, 0); return s ? s : dupstr("");
}

/* Python json.dumps(obj, ensure_ascii=False, default=str) with default separators. */
static void py_json_escape(buf_t *b, const char *s) {
    bput(b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '"')        bputs(b, "\\\"");
        else if (c == '\\')  bputs(b, "\\\\");
        else if (c == '\b')  bputs(b, "\\b");
        else if (c == '\f')  bputs(b, "\\f");
        else if (c == '\n')  bputs(b, "\\n");
        else if (c == '\r')  bputs(b, "\\r");
        else if (c == '\t')  bputs(b, "\\t");
        else if (c < 0x20) { static const char hx[] = "0123456789abcdef"; bputs(b, "\\u00"); bput(b, hx[c >> 4]); bput(b, hx[c & 15]); }
        else bput(b, (char)c);  /* ensure_ascii=False: UTF-8 passes through */
    }
    bput(b, '"');
}
static void py_dumps_node(buf_t *b, const json_t *n) {
    if (!n || n->type == JSON_NULL) { bputs(b, "null"); return; }
    switch (n->type) {
        case JSON_NULL:  bputs(b, "null"); break;
        case JSON_BOOL:  bputs(b, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64];
            if (n->num_val == (double)(long long)n->num_val) snprintf(tmp, sizeof(tmp), "%lld", (long long)n->num_val);
            else { int prec; for (prec = 1; prec <= 17; prec++) { snprintf(tmp, sizeof(tmp), "%.*g", prec, n->num_val); if (strtod(tmp, NULL) == n->num_val) break; } }
            bputs(b, tmp); break;
        }
        case JSON_STRING: py_json_escape(b, n->str_val ? n->str_val : ""); break;
        case JSON_ARRAY: {
            bput(b, '[');
            for (size_t i = 0; i < json_len(n); i++) { if (i) bputs(b, ", "); py_dumps_node(b, json_get(n, i)); }
            bput(b, ']'); break;
        }
        case JSON_OBJECT: {
            bput(b, '{');
            for (size_t i = 0; i < json_object_size(n); i++) {
                if (i) bputs(b, ", ");
                const char *k = json_object_get_key_at(n, i);
                py_json_escape(b, k ? k : "");
                bputs(b, ": ");
                py_dumps_node(b, json_object_get_at(n, i));
            }
            bput(b, '}'); break;
        }
    }
}
static char *py_dumps(const json_t *n) { buf_t b = {0}; py_dumps_node(&b, n); return bdone(&b); }

/* _stringify_tool_content, returning a malloc'd UTF-8 string. */
static char *stringify(const json_t *content) {
    if (!content || content->type == JSON_NULL) return dupstr("");
    if (content->type == JSON_STRING) return dupstr(content->str_val ? content->str_val : "");
    if (content->type == JSON_ARRAY) {
        buf_t b = {0};
        for (size_t i = 0; i < json_len(content); i++) {
            if (i) bput(&b, '\n');
            const json_t *item = json_get(content, i);
            if (item && item->type == JSON_OBJECT) {
                const json_t *txt = json_obj_get(item, "text");
                if (txt && txt->type == JSON_STRING) bputs(&b, txt->str_val ? txt->str_val : "");
                else { char *s = py_dumps(item); bputs(&b, s); free(s); }
            } else {
                if (!item || item->type == JSON_NULL) bputs(&b, "None");  /* str(None) */
                else { char *s = py_str(item); bputs(&b, s); free(s); }
            }
        }
        return bdone(&b);
    }
    if (content->type == JSON_OBJECT) return py_dumps(content);
    return py_str(content);
}

struct json_t *w15_delegateto_c3b7d4_stringify(const struct json_t *content) {
    char *s = stringify(content);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

static char *strip_lower(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = 0;
    return out;
}
static int starts_any_exact(const char *s, const char *const *vals, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strcmp(s, vals[i])) return 1;
    return 0;
}
static int starts_any(const char *s, const char *const *pfx, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strncmp(s, pfx[i], strlen(pfx[i]))) return 1;
    return 0;
}

struct json_t *w15_delegateto_c3b7d4_looks_error(const struct json_t *content) {
    char *text = stringify(content);
    if (!text || !text[0]) { free(text); return json_bool(false); }
    int verdict = 0;
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '{' || *p == '[') {
        json_t *parsed = json_parse(text, NULL);
        if (parsed && parsed->type == JSON_OBJECT) {
            const json_t *err = json_obj_get(parsed, "error");
            if (jtruthy(err)) verdict = 1;
            if (!verdict) {
                const json_t *st = json_obj_get(parsed, "status");
                char *sv = jtruthy(st) ? py_str(st) : dupstr("");
                char *low = strip_lower(sv);
                static const char *const BAD[] = {"error", "failed", "failure", "timeout"};
                if (starts_any_exact(low, BAD, 4)) verdict = 1;  /* exact set membership */
                free(low); free(sv);
            }
        }
        json_free(parsed);
    }
    if (!verdict) {
        const char *e = text;
        while (*e && *e != '\n' && *e != '\r') e++;
        size_t n = (size_t)(e - text);
        char *line = (char *)malloc(n + 1);
        memcpy(line, text, n); line[n] = 0;
        char *low = strip_lower(line);
        static const char *const PFX[] = {"error:", "failed:", "traceback ", "exception:"};
        if (starts_any(low, PFX, 4)) verdict = 1;
        free(low); free(line);
    }
    free(text);
    return json_bool(verdict ? true : false);
}

