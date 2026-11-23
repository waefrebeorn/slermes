/*
 * tts_tool_lifecycle.c - faithful C11 port of the deterministic lease/cache slice
 * of tools/tts_tool_lifecycle.py (gate w11_ttstoolli_234137).
 *
 * PoP: w11_ttstoolli_234137_keep_warm_seconds @ tools/tts_tool_lifecycle.py:_keep_warm_seconds
 * PoP: w11_ttstoolli_234137_release_provider @ tools/tts_tool_lifecycle.py:release_tts_provider
 * PoP: w11_ttstoolli_234137_warm_noop @ tools/tts_tool_lifecycle.py:warm_tts_provider
 * PoP: w11_ttstoolli_234137_lease_op @ tools/tts_tool_lifecycle.py:acquire_tts_lease
 * PoP: w11_ttstoolli_234137_lease_op @ tools/tts_tool_lifecycle.py:release_tts_lease
 * PoP: w11_ttstoolli_234137_lease_op @ tools/tts_tool_lifecycle.py:tts_lease_holders
 * PoP: w11_ttstoolli_234137_warmer_names @ tools/tts_tool_lifecycle.py:_local_tts_warmers
 *
 * Real model loading, timers, threads and plugin/command hooks are process-bound;
 * the oracle injects deterministic seams for those dependencies.
 */
#include "noc_w11_ttstoolli_234137.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_KEEP_WARM_SECONDS 60.0

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* (value or "").lower().strip() */
static char *provider_name(const json_t *v) {
    const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    if (o) {
        for (size_t i = 0; i < b - a; i++) o[i] = (char)tolower((unsigned char)s[a + i]);
        o[b - a] = 0;
    }
    return o;
}

/* Python float(raw) for the config scalar. */
static int py_float(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        double x = strtod(v->str_val ? v->str_val : "", &end);
        if (end == v->str_val || *end != '\0') return 0;
        *out = x;
        return 1;
    }
    return 0;
}

json_t *w11_ttstoolli_234137_keep_warm_seconds(const json_t *raw) {
    double v = DEFAULT_KEEP_WARM_SECONDS;
    if (raw && raw->type != JSON_NULL && py_float(raw, &v)) {
        if (v < 0.0) v = 0.0;
    } else {
        v = DEFAULT_KEEP_WARM_SECONDS;
    }
    return json_number(v);
}

/* Sum resident models in caches matching `name` ("" -> every cache). */
static long long released_count(const char *name, const json_t *cache_sizes) {
    long long total = 0;
    if (!json_is_object(cache_sizes)) return 0;
    for (size_t i = 0; i < json_object_size(cache_sizes); i++) {
        const char *k = json_object_get_key_at(cache_sizes, i);
        if ((!name || !*name) || (k && !strcmp(k, name))) {
            json_t *v = json_object_get_at(cache_sizes, i);
            if (v && v->type == JSON_NUMBER) total += (long long)v->num_val;
        }
    }
    return total;
}

json_t *w11_ttstoolli_234137_release_provider(const json_t *provider, const json_t *cache_sizes) {
    char *name = provider_name(provider);
    long long released = released_count(name, cache_sizes);
    free(name);
    json_t *out = json_object();
    json_set(out, "released", json_number((double)released));
    return out;
}

json_t *w11_ttstoolli_234137_warm_noop(const json_t *provider) {
    char *name = provider_name(provider);
    json_t *out = json_object();
    json_set(out, "provider", json_string(name ? name : ""));
    json_set(out, "warmed", json_bool(false));
    json_set(out, "action", json_string("noop"));
    free(name);
    return out;
}

/* ---------- lease set ---------- */
typedef struct { char **v; size_t n, cap; } Leases;
static void leases_add(Leases *s, const char *x) {
    for (size_t i = 0; i < s->n; i++) if (!strcmp(s->v[i], x)) return;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        char **p = (char **)realloc(s->v, nc * sizeof(char *));
        if (!p) return;
        s->v = p; s->cap = nc;
    }
    s->v[s->n++] = strdup(x);
}
static void leases_discard(Leases *s, const char *x) {
    for (size_t i = 0; i < s->n; i++) {
        if (!strcmp(s->v[i], x)) {
            free(s->v[i]);
            for (size_t j = i + 1; j < s->n; j++) s->v[j - 1] = s->v[j];
            s->n--;
            return;
        }
    }
}
static int lease_has(const Leases *s, const char *x) {
    for (size_t i = 0; i < s->n; i++) if (!strcmp(s->v[i], x)) return 1;
    return 0;
}
static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x, y);
}
static json_t *leases_json(Leases *s) {
    qsort(s->v, s->n, sizeof(char *), cmp_str);
    json_t *arr = json_array();
    for (size_t i = 0; i < s->n; i++) json_append(arr, json_string(s->v[i]));
    return arr;
}
static void leases_free(Leases *s) {
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL; s->n = s->cap = 0;
}

json_t *w11_ttstoolli_234137_lease_op(const char *op, const char *lease,
                                      const json_t *leases, const json_t *cache_sizes,
                                      double keep_warm) {
    Leases s = {0};
    if (json_is_array(leases))
        for (size_t i = 0; i < json_len(leases); i++) {
            json_t *v = json_get(leases, i);
            if (v && v->type == JSON_STRING) leases_add(&s, v->str_val ? v->str_val : "");
        }
    const char *l = lease ? lease : "";
    json_t *result = json_object();
    if (op && !strcmp(op, "acquire")) {
        leases_add(&s, l);
        result = w11_ttstoolli_234137_warm_noop(NULL);
        json_set(result, "leases", json_number((double)s.n));
    } else if (op && !strcmp(op, "release")) {
        int held = lease_has(&s, l);
        leases_discard(&s, l);
        long long released = 0;
        if (held && s.n == 0 && keep_warm <= 0.0)
            released = released_count("", cache_sizes);
        json_set(result, "leases", json_number((double)s.n));
        json_set(result, "released", json_number((double)released));
    } else { /* holders */
        result = leases_json(&s);
    }
    json_t *out = json_object();
    json_set(out, "result", result);
    json_set(out, "holders", leases_json(&s));
    leases_free(&s);
    return out;
}

json_t *w11_ttstoolli_234137_warmer_names(void) {
    json_t *arr = json_array();
    json_append(arr, json_string("kittentts"));
    json_append(arr, json_string("piper"));
    return arr;
}
