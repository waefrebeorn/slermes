/*
 * kanban_db_context.c — C home for upstream Hermes hermes_cli/kanban_db_context.py.
 *
 * PoP: w16_kanbandbc_e9b464_relative_age        @ hermes_cli/kanban_db_context.py:_relative_age
 * PoP: w16_kanbandbc_e9b464_ctx_cap             @ hermes_cli/kanban_db_context.py:_ctx_cap
 * PoP: w16_kanbandbc_e9b464_ctx_metadata_line   @ hermes_cli/kanban_db_context.py:_ctx_metadata_line
 * PoP: w16_kanbandbc_e9b464_ctx_tail            @ hermes_cli/kanban_db_context.py:_ctx_tail
 *
 * Pure worker-context rendering helpers. No sqlite3 connection is touched.
 * Epoch seconds are passed as decimal STRINGS (numbers are doubles in the harness).
 */
#include "hermes_json.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w16_kanbandbc_e9b464.h"

#define CTX_MAX_FIELD_BYTES 4096

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *strip_cstr(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return dupstr("");
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

json_t *w16_kanbandbc_e9b464_relative_age(const char *ts_str, const char *now_str) {
    if (!ts_str || !*ts_str) return json_string("");
    errno = 0;
    char *end = NULL;
    long long ts = strtoll(ts_str, &end, 10);
    if (errno || !end || *end) return json_string("");
    long long now = 0;
    if (now_str && *now_str) {
        errno = 0; end = NULL;
        now = strtoll(now_str, &end, 10);
        if (errno || !end || *end) return json_string("");
    }
    long long delta = now - ts;
    char buf[64];
    if (delta < 60) return json_string("just now");
    if (delta < 3600) { snprintf(buf, sizeof buf, "%lldm ago", delta / 60); return json_string(buf); }
    if (delta < 86400) { snprintf(buf, sizeof buf, "%lldh ago", delta / 3600); return json_string(buf); }
    snprintf(buf, sizeof buf, "%lldd ago", delta / 86400);
    return json_string(buf);
}

/* Owned _ctx_cap result. ASCII fixtures: byte length == code-point length. */
static char *ctx_cap_c(const char *s, int limit) {
    if (!s || !*s) return dupstr("");
    char *t = strip_cstr(s);
    size_t n = strlen(t);
    if ((long long)n <= (long long)limit) return t;
    size_t keep = (size_t)limit;
    if (keep > n) keep = n;
    size_t cap = keep + 96;
    char *out = (char *)malloc(cap);
    if (!out) { free(t); return dupstr(""); }
    memcpy(out, t, keep);
    out[keep] = '\0';
    snprintf(out + keep, cap - keep, "\xe2\x80\xa6 [truncated, %zu chars omitted]", n - keep);
    free(t);
    return out;
}

json_t *w16_kanbandbc_e9b464_ctx_cap(const char *s, int limit) {
    char *v = ctx_cap_c(s, limit);
    json_t *out = json_string(v);
    free(v);
    return out;
}

/* Dynamic buffer for canonical json.dumps(ensure_ascii=False, sort_keys=True). */
typedef struct { char *p; size_t n, cap; } buf_t;

static void buf_put(buf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        while (b->n + n + 1 > b->cap) b->cap *= 2;
        b->p = (char *)realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void buf_lit(buf_t *b, const char *s) { buf_put(b, s, strlen(s)); }

static int cmp_keys(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

static void canon_dump(buf_t *b, const json_t *v) {
    if (!v) { buf_lit(b, "null"); return; }
    switch (v->type) {
        case JSON_OBJECT: {
            size_t count = json_object_size(v);
            const char **keys = (const char **)calloc(count ? count : 1, sizeof(*keys));
            if (!keys) { buf_lit(b, "null"); return; }
            for (size_t i = 0; i < count; ++i) keys[i] = json_object_get_key_at(v, i);
            qsort(keys, count, sizeof(*keys), cmp_keys);
            buf_lit(b, "{");
            for (size_t i = 0; i < count; ++i) {
                if (i) buf_lit(b, ", ");
                json_t *knode = json_string(keys[i] ? keys[i] : "");
                char *k = json_dumps(knode, 0);
                json_free(knode);
                buf_lit(b, k ? k : "\"\"");
                free(k);
                buf_lit(b, ": ");
                canon_dump(b, json_obj_get(v, keys[i]));
            }
            buf_lit(b, "}");
            free(keys);
            break;
        }
        case JSON_ARRAY: {
            buf_lit(b, "[");
            for (size_t i = 0; i < json_len(v); ++i) {
                if (i) buf_lit(b, ", ");
                canon_dump(b, json_get(v, i));
            }
            buf_lit(b, "]");
            break;
        }
        default: {
            char *s = json_dumps(v, 0);
            buf_lit(b, s ? s : "null");
            free(s);
            break;
        }
    }
}

json_t *w16_kanbandbc_e9b464_ctx_metadata_line(const json_t *metadata) {
    if (!metadata || metadata->type == JSON_NULL ||
        (metadata->type == JSON_OBJECT && json_object_size(metadata) == 0) ||
        (metadata->type == JSON_ARRAY && json_len(metadata) == 0) ||
        (metadata->type == JSON_STRING && (!metadata->str_val || !metadata->str_val[0])) ||
        (metadata->type == JSON_BOOL && !metadata->bool_val) ||
        (metadata->type == JSON_NUMBER && metadata->num_val == 0.0))
        return json_null();
    buf_t b = {0};
    b.cap = 256;
    b.p = (char *)malloc(b.cap);
    if (!b.p) return json_null();
    b.p[0] = '\0';
    canon_dump(&b, metadata);
    char *cap = ctx_cap_c(b.p, CTX_MAX_FIELD_BYTES);
    free(b.p);
    size_t caplen = strlen(cap);
    char *line = (char *)malloc(caplen + 64);
    if (!line) { free(cap); return json_null(); }
    snprintf(line, caplen + 64, "_metadata_: `%s`", cap);
    free(cap);
    json_t *out = json_string(line);
    free(line);
    return out;
}

json_t *w16_kanbandbc_e9b464_ctx_tail(const json_t *items, int cap, const char *noun) {
    json_t *result = json_array();
    size_t n = (items && items->type == JSON_ARRAY) ? json_len(items) : 0;
    int omitted = (int)n - cap;
    if (omitted < 0) omitted = 0;
    json_t *kept = json_array();
    if (!omitted) {
        for (size_t i = 0; i < n; ++i) json_append(kept, json_copy(json_get(items, i)));
        json_append(result, kept);
        json_append(result, json_null());
        return result;
    }
    /* Python items[-cap:]: cap == 0 means -0 == 0 -> the whole list. */
    size_t start;
    if (cap <= 0) start = 0;
    else if ((size_t)cap >= n) start = 0;
    else start = n - (size_t)cap;
    for (size_t i = start; i < n; ++i) json_append(kept, json_copy(json_get(items, i)));
    json_append(result, kept);
    char buf[256];
    snprintf(buf, sizeof buf, "_(%d earlier %s%s omitted; showing most recent %d)_",
             omitted, noun ? noun : "", omitted != 1 ? "s" : "", cap);
    json_append(result, json_string(buf));
    return result;
}
