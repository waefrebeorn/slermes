/*
 * jobs_normalize.c — real C11 port of upstream Hermes cron/jobs_normalize.py.
 *
 * PoP: jobsnorm_normalize_optional_text @ cron/jobs_normalize.py:_normalize_job_optional_text
 * PoP: jobsnorm_normalize_base_url      @ cron/jobs_normalize.py:_normalize_base_url
 * PoP: jobsnorm_normalize_str_list      @ cron/jobs_normalize.py:_normalize_str_list
 * PoP: jobsnorm_normalize_context_from  @ cron/jobs_normalize.py:_normalize_context_from
 * PoP: jobsnorm_normalize_failure_deliver @ cron/jobs_normalize.py:_normalize_failure_deliver
 *
 * Self-contained (only libc + libjson) so the port object links with an
 * explicit "<this>.o lib/libjson/json.o" gate object list.
 */
#include "noc_jobsnorm.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python str.strip(): ASCII whitespace. Unicode-only whitespace is out of
 * scope (documented in the oracle notes); fixtures exercise ASCII + UTF-8
 * non-whitespace text. */
static bool is_py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *strip_ws(const char *s) {
    size_t a = 0, b;
    if (!s) return NULL;
    b = strlen(s);
    while (a < b && is_py_ws((unsigned char)s[a])) a++;
    while (b > a && is_py_ws((unsigned char)s[b - 1])) b--;
    return dup_n(s + a, b - a);
}

static char *rstrip_char(const char *s, char ch) {
    size_t b;
    if (!s) return NULL;
    b = strlen(s);
    while (b > 0 && s[b - 1] == ch) b--;
    return dup_n(s, b);
}

/* Python str() for JSON scalars that reach the list normalizers. Integer
 * literals print as decimal; non-integral doubles use the shortest of
 * %.15g/%.16g/%.17g that round-trips (matching Python repr for common values). */
static char *num_py_str(double v) {
    char buf[64];
    if (v == 0.0) return dup_n("0", 1);
    if (isfinite(v) && floor(v) == v && v >= -9.22e18 && v <= 9.22e18) {
        snprintf(buf, sizeof(buf), "%lld", (long long)v);
        return dup_n(buf, strlen(buf));
    }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, v);
        if (strtod(buf, NULL) == v) break;
    }
    return dup_n(buf, strlen(buf));
}

static char *py_str(const json_t *v) {
    if (!v) return dup_n("None", 4);
    switch (v->type) {
    case JSON_NULL:   return dup_n("None", 4);
    case JSON_BOOL:   return v->bool_val ? dup_n("True", 4) : dup_n("False", 5);
    case JSON_NUMBER: return num_py_str(v->num_val);
    case JSON_STRING: return dup_n(v->str_val ? v->str_val : "", v->str_val ? strlen(v->str_val) : 0);
    case JSON_ARRAY: {
        size_t n = json_len(v), i, cap = 2, used = 0;
        char *buf = (char *)malloc(cap);
        if (!buf) return NULL;
        buf[0] = '['; used = 1; buf[1] = '\0';
        for (i = 0; i < n; i++) {
            char *s = py_str(json_get(v, i));
            size_t sl = s ? strlen(s) : 0;
            size_t need = used + sl + 4;
            if (need > cap) {
                while (cap < need) cap *= 2;
                buf = (char *)realloc(buf, cap);
                if (!buf) { free(s); return NULL; }
            }
            if (i) { buf[used++] = ','; buf[used++] = ' '; }
            if (sl) memcpy(buf + used, s, sl);
            used += sl;
            buf[used] = '\0';
            free(s);
        }
        buf[used++] = ']'; buf[used] = '\0';
        return buf;
    }
    case JSON_OBJECT: {
        size_t n = json_object_size(v), i, cap = 2, used = 0;
        char *buf = (char *)malloc(cap);
        if (!buf) return NULL;
        buf[0] = '{'; used = 1; buf[1] = '\0';
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            char *s = py_str(json_object_get_at(v, i));
            size_t sl = s ? strlen(s) : 0;
            size_t kl = k ? strlen(k) : 0;
            size_t need = used + kl + sl + 8;
            if (need > cap) {
                while (cap < need) cap *= 2;
                buf = (char *)realloc(buf, cap);
                if (!buf) { free(s); return NULL; }
            }
            if (i) { buf[used++] = ','; buf[used++] = ' '; }
            buf[used++] = '\'';
            if (kl) memcpy(buf + used, k, kl);
            used += kl;
            memcpy(buf + used, "': ", 3); used += 3;
            if (sl) memcpy(buf + used, s, sl);
            used += sl;
            buf[used] = '\0';
            free(s);
        }
        buf[used++] = '}'; buf[used] = '\0';
        return buf;
    }
    }
    return dup_n("None", 4);
}

/* PoP: jobsnorm_normalize_optional_text @ cron/jobs_normalize.py:_normalize_job_optional_text */
char *jobsnorm_normalize_optional_text(const char *value, bool strip_trailing_slash) {
    char *s, *t;
    if (!value) return NULL;                 /* not isinstance(value, str) */
    s = strip_ws(value);
    if (!s) return NULL;
    if (strip_trailing_slash) {
        t = rstrip_char(s, '/');
        free(s);
        s = t;
        if (!s) return NULL;
    }
    if (*s == '\0') { free(s); return NULL; } /* `or None` */
    return s;
}

/* PoP: jobsnorm_normalize_base_url @ cron/jobs_normalize.py:_normalize_base_url */
char *jobsnorm_normalize_base_url(const char *value) {
    return jobsnorm_normalize_optional_text(value, true);
}

/* PoP: jobsnorm_normalize_str_list @ cron/jobs_normalize.py:_normalize_str_list */
json_t *jobsnorm_normalize_str_list(const json_t *items) {
    json_t *out = json_array();
    size_t n, i;
    if (!out) return NULL;
    if (!json_is_array(items)) { json_free(out); return NULL; }
    n = json_len(items);
    for (i = 0; i < n; i++) {
        char *s = py_str(json_get(items, i));
        char *t = strip_ws(s);
        free(s);
        if (t && *t) json_append(out, json_string(t));
        free(t);
    }
    if (json_len(out) == 0) { json_free(out); return NULL; }  /* `or None` */
    return out;
}

/* PoP: jobsnorm_normalize_context_from @ cron/jobs_normalize.py:_normalize_context_from */
json_t *jobsnorm_normalize_context_from(const json_t *value) {
    json_t *wrapped, *res;
    if (json_is_string(value)) {
        wrapped = json_array();
        if (!wrapped) return NULL;
        json_append(wrapped, json_copy(value));
        res = jobsnorm_normalize_str_list(wrapped);
        json_free(wrapped);
        return res;
    }
    if (json_is_array(value)) return jobsnorm_normalize_str_list(value);
    return NULL;
}

/* PoP: jobsnorm_normalize_failure_deliver @ cron/jobs_normalize.py:_normalize_failure_deliver */
char *jobsnorm_normalize_failure_deliver(const json_t *value) {
    if (json_is_array(value)) {
        size_t n = json_len(value), i, cap = 16, used = 0;
        char *buf = (char *)malloc(cap);
        if (!buf) return NULL;
        buf[0] = '\0';
        for (i = 0; i < n; i++) {
            char *s = py_str(json_get(value, i));
            char *t = strip_ws(s);
            free(s);
            if (t && *t) {
                size_t tl = strlen(t);
                size_t need = used + tl + 2;
                if (need > cap) {
                    while (cap < need) cap *= 2;
                    buf = (char *)realloc(buf, cap);
                    if (!buf) { free(t); return NULL; }
                }
                if (used) buf[used++] = ',';
                memcpy(buf + used, t, tl); used += tl;
                buf[used] = '\0';
            }
            free(t);
        }
        if (!used) { free(buf); return NULL; }  /* `or None` */
        return buf;
    }
    return jobsnorm_normalize_optional_text(
        json_is_string(value) ? json_string_value(value) : NULL, false);
}

