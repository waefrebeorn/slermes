/*
 * _search_worker.c - C11 port of upstream plugins/web/ddgs/_search_worker.py
 * (gate w24_searchwork_58ae49).
 *
 * PoP: w24_searchwork_58ae49_normalize @ plugins/web/ddgs/_search_worker.py:main
 * PoP: w24_searchwork_58ae49_handle @ plugins/web/ddgs/_search_worker.py:main
 * PoP: w24_searchwork_58ae49_constants @ plugins/web/ddgs/_search_worker.py:_TEST_HOOKS
 *
 * What is ported is the worker's decision: request normalisation, the
 * env-gated test hooks, and which envelope plus exit code each branch yields.
 * The process seams stay outside: stdin/stdout, the
 * HERMES_DDGS_ALLOW_TEST_HOOKS gate and the provider call.  The provider's
 * result is the seam value (an echo of the normalised query and limit), and
 * the two hooks that exist to prove a hung child is killable answer their
 * post-sleep envelope immediately instead of blocking for 30 seconds.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_searchwork_58ae49.h"

/* Python truthiness of a JSON value. */
static int sw_truthy(const json_t *v) {
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

/* str(value) for the scalar shapes; NULL for containers (seam). */
static char *sw_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    return NULL;
}

/* repr(value) for the scalar shapes -- the `unknown test_hook: {hook!r}` text. */
static char *sw_repr(const json_t *v) {
    char buf[128];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t i, o = 0, need = strlen(s) * 2 + 4;
        char *out = (char *)malloc(need);
        int quote = strchr(s, '\'') ? '"' : '\'';
        if (!out) return NULL;
        out[o++] = (char)quote;
        for (i = 0; s[i]; i++) {
            unsigned char c = (unsigned char)s[i];
            if ((int)c == quote || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
            else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
            else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
            else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
            else out[o++] = (char)c;
        }
        out[o++] = (char)quote;
        out[o] = '\0';
        return out;
    }
    return NULL;
}

/* int(value) for bool/number/str, following int()'s string literal rules
 * (surrounding whitespace, sign, decimal digits with single underscores). */
static int sw_int(const json_t *v, long long *out) {
    const char *p;
    int neg = 0, digits = 0, prev_digit = 0;
    long long acc = 0;

    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type != JSON_STRING) return 0;          /* None/list/dict: TypeError */
    p = v->str_val ? v->str_val : "";
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (*p == '+' || *p == '-') { if (*p == '-') neg = 1; p++; }
    for (;;) {
        if (*p >= '0' && *p <= '9') { acc = acc * 10 + (*p - '0'); p++; digits = 1; prev_digit = 1; }
        else if (*p == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) return 0;
            p++; prev_digit = 0;
        } else break;
    }
    if (!digits) return 0;                          /* "" / "abc" -> ValueError */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (*p) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

json_t *w24_searchwork_58ae49_normalize(const json_t *req) {
    const json_t *qv = json_obj_get(req, "query");
    const json_t *sv = json_obj_get(req, "safe_limit");
    char *query;
    long long limit = 1;
    json_t *out;

    query = sw_truthy(qv) ? sw_str(qv) : strdup("");
    if (!query) return json_null();                 /* str() over a container */
    if (sw_truthy(sv) && !sw_int(sv, &limit)) {     /* int(...) raised */
        free(query);
        return json_null();
    }
    if (limit < 1) limit = 1;
    out = json_object();
    json_set(out, "query", json_string(query));
    json_set(out, "safe_limit", json_number((double)limit));
    free(query);
    return out;
}

static json_t *sw_fail(const char *error, int code) {
    json_t *res = json_object();
    json_t *env = json_object();
    json_set(env, "ok", json_bool(false));
    json_set(env, "error", json_string(error));
    json_set(res, "code", json_number((double)code));
    json_set(res, "envelope", env);
    return res;
}

json_t *w24_searchwork_58ae49_handle(const json_t *req, int hooks_enabled) {
    const json_t *hook = json_obj_get(req, "test_hook");
    char *hs;
    json_t *out;

    if (sw_truthy(hook)) {
        char *msg;
        size_t need;
        hs = sw_str(hook);
        if (!hooks_enabled) {
            free(hs);
            return sw_fail("test_hook refused (hooks not enabled)", 3);
        }
        if (hs && !strcmp(hs, "empty")) {
            json_t *env = json_object();
            free(hs);
            out = json_object();
            json_set(env, "ok", json_bool(true));
            json_set(env, "results", json_array());
            json_set(out, "code", json_number(0.0));
            json_set(out, "envelope", env);
            return out;
        }
        if (hs && (!strcmp(hs, "sleep") || !strcmp(hs, "gil"))) {
            /* _hang(block, name): the block returns after its 30s budget only
             * if the child survived; the seam answers that envelope at once. */
            need = strlen(hs) + 32;
            msg = (char *)malloc(need);
            if (!msg) { free(hs); return json_null(); }
            snprintf(msg, need, "%s hook returned unexpectedly", hs);
            free(hs);
            out = sw_fail(msg, 1);
            free(msg);
            return out;
        }
        {
            char *rp = sw_repr(hook);
            need = (rp ? strlen(rp) : 0) + 24;
            msg = (char *)malloc(need);
            if (!msg) { free(hs); free(rp); return json_null(); }
            snprintf(msg, need, "unknown test_hook: %s", rp ? rp : "");
            free(rp);
            free(hs);
            out = sw_fail(msg, 1);
            free(msg);
            return out;
        }
    }

    {
        json_t *norm = w24_searchwork_58ae49_normalize(req);
        json_t *env, *results, *hit;
        const char *query;
        double limit;
        if (!norm || norm->type == JSON_NULL) { json_free(norm); return json_null(); }
        query = json_get_str(norm, "query", "");
        limit = json_get_num(norm, "safe_limit", 1.0);
        hit = json_object();
        json_set(hit, "query", json_string(query));
        json_set(hit, "limit", json_number(limit));
        results = json_array();
        json_append(results, hit);
        env = json_object();
        json_set(env, "ok", json_bool(true));
        json_set(env, "results", results);
        out = json_object();
        json_set(out, "code", json_number(0.0));
        json_set(out, "envelope", env);
        json_free(norm);
        return out;
    }
}

json_t *w24_searchwork_58ae49_constants(void) {
    static const char *const hooks[] = { "empty", "gil", "sleep" };
    json_t *o = json_object(), *arr = json_array(), *texts = json_object();
    size_t i;

    for (i = 0; i < sizeof hooks / sizeof hooks[0]; i++)
        json_append(arr, json_string(hooks[i]));
    json_set(o, "test_hooks", arr);
    json_set(o, "hooks_env", json_string(W24_SEARCHWORK_58AE49_HOOKS_ENV));
    json_set(texts, "refused", json_string("test_hook refused (hooks not enabled)"));
    json_set(texts, "unknown_prefix", json_string("unknown test_hook: "));
    json_set(texts, "hang_suffix", json_string(" hook returned unexpectedly"));
    json_set(texts, "invalid_prefix", json_string("invalid request: "));
    json_set(o, "texts", texts);
    return o;
}
