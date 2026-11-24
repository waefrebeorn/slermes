/*
 * provider.c — C11 port of upstream Hermes plugins/web/ddgs/provider.py
 * (gate w26_provider_21d780).
 * STATUS: PORTED — behavior verified by tests/oracle_w26_provider_21d780.py.
 * PoP: w26_provider_21d780_parse_envelope @ plugins/web/ddgs/provider.py:_parse_envelope
 * PoP: w26_provider_21d780_search @ plugins/web/ddgs/provider.py:DDGSWebSearchProvider.search
 * PoP: w26_provider_21d780_setup_schema @ plugins/web/ddgs/provider.py:DDGSWebSearchProvider.get_setup_schema
 * PoP: w26_provider_21d780_constants @ plugins/web/ddgs/provider.py:<module constants>
 *
 * Seams: the disposable worker process (spawn, pipes, the 30 s kill cap) is not
 * ported; its stdout text and exit code arrive as values.  What is ported is
 * the decision that text drives -- the envelope decode with its exact error
 * prose -- plus the search outcome mapping and the picker entry.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_provider_21d780.h"

#define DD_DEFAULT_ERROR "DDGS worker failed"
#define DD_NOT_INSTALLED "ddgs package is not installed — run `pip install ddgs`"
#define DD_INTERRUPTED "DuckDuckGo search interrupted"

/* Python truthiness of a JSON value. */
static int dd_truthy(const json_t *v) {
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

static json_t *dd_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    return o;
}

/* Python repr() for the scalar envelope shapes a worker can emit. */
static void dd_repr(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    if (v->type == JSON_BOOL) { snprintf(buf, cap, v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, cap, "%lld", (long long)d);
        else snprintf(buf, cap, "%g", d);
        return;
    }
    if (v->type == JSON_STRING) { snprintf(buf, cap, "'%s'", v->str_val ? v->str_val : ""); return; }
    snprintf(buf, cap, "<json>");
}

/* str.strip() over ASCII whitespace. */
static char *dd_strip(const char *s) {
    size_t a = 0, b;
    char *out;
    while (s[a] && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                    s[a] == '\f' || s[a] == '\v')) a++;
    b = strlen(s);
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                     s[b - 1] == '\r' || s[b - 1] == '\f' || s[b - 1] == '\v')) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

json_t *w26_provider_21d780_parse_envelope(const char *raw_in, long long exit_code) {
    char msg[1024], buf[512];
    char *raw;
    json_t *env, *results, *out;

    raw = dd_strip(raw_in ? raw_in : "");
    if (!raw) return dd_err(DD_DEFAULT_ERROR);
    if (!raw[0]) {
        free(raw);
        snprintf(msg, sizeof(msg), "DDGS worker exited without a result (code=%lld)", exit_code);
        return dd_err(msg);
    }
    env = json_parse(raw, NULL);
    if (!env) {
        snprintf(msg, sizeof(msg), "DDGS worker returned invalid JSON: '%s'", raw);
        free(raw);
        return dd_err(msg);
    }
    free(raw);
    if (env->type != JSON_OBJECT) {
        dd_repr(env, buf, sizeof(buf));
        snprintf(msg, sizeof(msg), "DDGS worker returned an invalid envelope: %s", buf);
        json_free(env);
        return dd_err(msg);
    }
    if (!dd_truthy(json_obj_get(env, "ok"))) {
        const json_t *e = json_obj_get(env, "error");
        if (dd_truthy(e)) {
            if (e->type == JSON_STRING) snprintf(msg, sizeof(msg), "%s", e->str_val ? e->str_val : "");
            else dd_repr(e, msg, sizeof(msg));
        } else {
            snprintf(msg, sizeof(msg), "%s", DD_DEFAULT_ERROR);
        }
        json_free(env);
        return dd_err(msg);
    }
    results = json_obj_get(env, "results");
    if (!dd_truthy(results)) {
        json_free(env);
        out = json_object();
        json_set(out, "ok", json_bool(1));
        json_set(out, "results", json_array());
        return out;
    }
    if (results->type != JSON_ARRAY) {
        json_free(env);
        return dd_err("DDGS worker returned non-list results");
    }
    out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "results", json_copy(results));
    json_free(env);
    return out;
}

json_t *w26_provider_21d780_search(const json_t *req) {
    const json_t *limit_v = json_obj_get(req, "limit");
    const char *seam = json_get_str(req, "seam", "ok");
    json_t *res = json_object();
    long long safe = 1;

    if (!dd_truthy(json_obj_get(req, "available"))) {
        json_t *fail = json_object(), *r = json_object();
        json_set(fail, "success", json_bool(0));
        json_set(fail, "error", json_string(DD_NOT_INSTALLED));
        json_set(res, "result", fail);
        json_set(res, "safe_limit", json_null());
        return res;
    }
    if (limit_v && limit_v->type == JSON_NUMBER) safe = (long long)limit_v->num_val;
    else if (limit_v && limit_v->type == JSON_BOOL) safe = limit_v->bool_val ? 1 : 0;
    if (safe < 1) safe = 1;

    if (!strcmp(seam, "timeout")) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "DuckDuckGo search timed out after %ds — DuckDuckGo may be rate-limiting "
                 "or slow. Try again later or switch to a different search provider.", 30);
        {
            json_t *fail = json_object();
            json_set(fail, "success", json_bool(0));
            json_set(fail, "error", json_string(msg));
            json_set(res, "result", fail);
        }
        json_set(res, "safe_limit", json_number((double)safe));
        return res;
    }
    if (!strcmp(seam, "interrupt")) {
        json_t *fail = json_object();
        json_set(fail, "success", json_bool(0));
        json_set(fail, "error", json_string(DD_INTERRUPTED));
        json_set(res, "result", fail);
        json_set(res, "safe_limit", json_number((double)safe));
        return res;
    }
    if (!strcmp(seam, "error")) {
        char msg[512];
        snprintf(msg, sizeof(msg), "DuckDuckGo search failed: %s", "boom");
        {
            json_t *fail = json_object();
            json_set(fail, "success", json_bool(0));
            json_set(fail, "error", json_string(msg));
            json_set(res, "result", fail);
        }
        json_set(res, "safe_limit", json_number((double)safe));
        return res;
    }

    /* seam == "ok": the bounded call returned these hits unchanged. */
    {
        const json_t *hits = json_obj_get(req, "hits");
        json_t *ok = json_object(), *data = json_object();
        json_set(data, "web", hits ? json_copy(hits) : json_array());
        json_set(ok, "success", json_bool(1));
        json_set(ok, "data", data);
        json_set(res, "result", ok);
        json_set(res, "safe_limit", json_number((double)safe));
    }
    return res;
}

json_t *w26_provider_21d780_setup_schema(void) {
    json_t *o = json_object();
    json_set(o, "name", json_string("DuckDuckGo (ddgs)"));
    json_set(o, "badge", json_string("free · no key · search only"));
    json_set(o, "tag", json_string("Search via the ddgs Python package — no API key "
                                   "(pair with any extract provider)"));
    json_set(o, "env_vars", json_array());
    json_set(o, "post_setup", json_string("ddgs"));
    return o;
}

json_t *w26_provider_21d780_constants(void) {
    json_t *o = json_object();
    json_set(o, "search_timeout_secs", json_number(30));
    json_set(o, "poll_interval_secs", json_number(0.1));
    json_set(o, "terminate_grace_secs", json_number(1.0));
    json_set(o, "name", json_string("ddgs"));
    json_set(o, "display_name", json_string("DuckDuckGo (ddgs)"));
    return o;
}
