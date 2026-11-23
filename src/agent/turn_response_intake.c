/*
 * turn_response_intake.c — C home for upstream Hermes agent/turn_response_intake.py.
 * PoP: w13_turnrespon_533ef3_coerce @ agent/turn_response_intake.py:_coerce_content_text
 *
 * The response-normalization helper that turns a non-str content payload into a
 * plain string (or the first truthy dict field verbatim).  Hook/verdict side
 * effects of normalize_model_response stay in Python.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_turnrespon_533ef3.h"

static char *tri_dup(const char *s) { size_t n = strlen(s); char *o = (char *)malloc(n + 1); memcpy(o, s, n + 1); return o; }
static int tri_truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count > 0;
}
static char *tri_py_str(json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return tri_dup("None");
    if (v->type == JSON_BOOL) return tri_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return tri_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%g", v->num_val);
        return tri_dup(buf);
    }
    return tri_dup("");
}

/* Python json.dumps(raw) with the default separators and ensure_ascii=True. */
typedef struct { char *b; size_t cap, len; } tri_buf;
static void tri_pc(tri_buf *b, char c) {
    if (b->len + 1 >= b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->b = (char *)realloc(b->b, b->cap);
    }
    b->b[b->len++] = c;
}
static void tri_ps(tri_buf *b, const char *s) { while (*s) tri_pc(b, *s++); }
static void tri_hex4(tri_buf *b, unsigned cp) {
    char t[8];
    snprintf(t, sizeof(t), "\\u%04x", cp & 0xFFFF);
    tri_ps(b, t);
}
static void tri_pstr(tri_buf *b, const char *s) {
    tri_pc(b, '"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p;) {
        unsigned char c = *p;
        if (c == '"') { tri_ps(b, "\\\""); p++; }
        else if (c == '\\') { tri_ps(b, "\\\\"); p++; }
        else if (c == '\b') { tri_ps(b, "\\b"); p++; }
        else if (c == '\f') { tri_ps(b, "\\f"); p++; }
        else if (c == '\n') { tri_ps(b, "\\n"); p++; }
        else if (c == '\r') { tri_ps(b, "\\r"); p++; }
        else if (c == '\t') { tri_ps(b, "\\t"); p++; }
        else if (c < 0x20) { tri_hex4(b, c); p++; }
        else if (c < 0x80) { tri_pc(b, (char)c); p++; }
        else {
            unsigned cp; int need;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; need = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 3; }
            else { cp = c; need = 0; }
            p++;
            for (int i = 0; i < need && *p; i++) { cp = (cp << 6) | (*p & 0x3F); p++; }
            if (cp <= 0xFFFF) tri_hex4(b, cp);
            else {
                unsigned v = cp - 0x10000;
                tri_hex4(b, 0xD800 + (v >> 10));
                tri_hex4(b, 0xDC00 + (v & 0x3FF));
            }
        }
    }
    tri_pc(b, '"');
}
static void tri_pnode(tri_buf *b, json_t *n) {
    if (!n) { tri_ps(b, "null"); return; }
    switch (n->type) {
        case JSON_NULL: tri_ps(b, "null"); break;
        case JSON_BOOL: tri_ps(b, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char t[64];
            if (n->num_val == (double)(long long)n->num_val) snprintf(t, sizeof(t), "%.0f", n->num_val);
            else snprintf(t, sizeof(t), "%g", n->num_val);
            tri_ps(b, t);
            break;
        }
        case JSON_STRING: tri_pstr(b, n->str_val); break;
        case JSON_ARRAY:
            tri_pc(b, '[');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i) tri_ps(b, ", ");
                tri_pnode(b, n->c.items[i]);
            }
            tri_pc(b, ']');
            break;
        case JSON_OBJECT:
            tri_pc(b, '{');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i) tri_ps(b, ", ");
                tri_pstr(b, n->c.keys[i]);
                tri_ps(b, ": ");
                tri_pnode(b, n->c.items[i]);
            }
            tri_pc(b, '}');
            break;
    }
}
static char *tri_py_dumps(json_t *n) {
    tri_buf b = {NULL, 0, 0};
    tri_pnode(&b, n);
    tri_pc(&b, 0);
    return b.b ? b.b : tri_dup("null");
}

json_t *w13_turnrespon_533ef3_coerce(json_t *raw) {
    if (raw && raw->type == JSON_OBJECT) {
        json_t *t = json_obj_get(raw, "text");
        if (tri_truthy(t)) return json_copy(t);
        json_t *c = json_obj_get(raw, "content");
        if (tri_truthy(c)) return json_copy(c);
        char *s = tri_py_dumps(raw);
        json_t *out = json_string(s);
        free(s);
        return out;
    }
    if (raw && raw->type == JSON_ARRAY) {
        size_t n = raw->c.count;
        char **parts = (char **)calloc(n ? n : 1, sizeof(char *));
        size_t m = 0;
        for (size_t i = 0; i < n; i++) {
            json_t *p = raw->c.items[i];
            if (p && p->type == JSON_STRING) { parts[m++] = tri_dup(p->str_val ? p->str_val : ""); continue; }
            if (p && p->type == JSON_OBJECT) {
                json_t *ty = json_obj_get(p, "type");
                if (ty && ty->type == JSON_STRING && ty->str_val && !strcmp(ty->str_val, "text")) {
                    json_t *tv = json_obj_get(p, "text");
                    parts[m++] = tv && tv->type == JSON_STRING ? tri_dup(tv->str_val) : tri_dup("");
                    continue;
                }
                if (json_obj_get(p, "text")) {
                    char *s = tri_py_str(json_obj_get(p, "text"));
                    parts[m++] = s;
                    continue;
                }
            }
        }
        size_t tot = 0;
        for (size_t i = 0; i < m; i++) tot += strlen(parts[i]) + (i ? 1 : 0);
        char *res = (char *)malloc(tot + 1);
        size_t k = 0;
        for (size_t i = 0; i < m; i++) {
            if (i) res[k++] = '\n';
            size_t L = strlen(parts[i]);
            memcpy(res + k, parts[i], L);
            k += L;
            free(parts[i]);
        }
        free(parts);
        res[k] = 0;
        json_t *out = json_string(res);
        free(res);
        return out;
    }
    char *s = tri_py_str(raw);
    json_t *out = json_string(s);
    free(s);
    return out;
}
