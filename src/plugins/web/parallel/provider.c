/*
 * provider.c - C11 port of upstream plugins/web/parallel/provider.py
 * (gate w25_provider_f2f12e).
 *
 * PoP: w25_provider_f2f12e_resolve_search_mode @ plugins/web/parallel/provider.py:_resolve_search_mode
 * PoP: w25_provider_f2f12e_search_plan @ plugins/web/parallel/provider.py:ParallelWebSearchProvider.search (beta.search call plan)
 * PoP: w25_provider_f2f12e_search_hits @ plugins/web/parallel/provider.py:ParallelWebSearchProvider.search (response -> web_hit rows)
 * PoP: w25_provider_f2f12e_constants @ plugins/web/parallel/provider.py:ParallelWebSearchProvider.<class constants>
 *
 * The Parallel SDK client, the PARALLEL_API_KEY lookup, the keyless ring and
 * the guarded runner are seams: the SDK reply is a fixed value, the keyless
 * decision is a fixed input. What is ported is what the values decide -- the
 * mode string, the capped call plan and the hit rows built from a reply.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_provider_f2f12e.h"

#define PROVIDER_SEARCH_LIMIT_CAP W25_PROVIDER_F2F12E_SEARCH_LIMIT_CAP

/* Python truthiness of a JSON value. */
static int pv_truthy(const json_t *v) {
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

static int pv_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.lower() for ASCII, then str.strip() of all whitespace. */
static void pv_lower_strip(const char *in, char *out, size_t cap) {
    size_t i = 0, n = 0, end;
    if (!in) in = "";
    for (; in[i] && n + 1 < cap; i++) {
        unsigned char c = (unsigned char)in[i];
        out[n++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    out[n] = '\0';
    end = n;
    while (end > 0 && pv_space((unsigned char)out[end - 1])) out[--end] = '\0';
    i = 0;
    while (out[i] && pv_space((unsigned char)out[i])) i++;
    if (i > 0) memmove(out, out + i, end - i + 1);
}

/* The three modes _resolve_search_mode accepts. */
static int pv_known_mode(const char *mode) {
    return !strcmp(mode, "fast") || !strcmp(mode, "one-shot") || !strcmp(mode, "agentic");
}

json_t *w25_provider_f2f12e_resolve_search_mode(const json_t *req) {
    const json_t *raw = req ? json_obj_get(req, "mode_env") : NULL;
    char buf[256];
    if (!raw || raw->type != JSON_STRING) return json_string("agentic");
    pv_lower_strip(raw->str_val, buf, sizeof(buf));
    return json_string(pv_known_mode(buf) ? buf : "agentic");
}

json_t *w25_provider_f2f12e_search_plan(const json_t *req) {
    const json_t *query = req ? json_obj_get(req, "query") : NULL;
    json_t *o = json_object(), *list = json_array();
    const json_t *mlimit = req ? json_obj_get(req, "limit") : NULL;
    double limit = 5.0, capped;

    if (mlimit && mlimit->type == JSON_NUMBER) limit = mlimit->num_val;
    else if (mlimit && mlimit->type == JSON_BOOL) limit = mlimit->bool_val ? 1.0 : 0.0;
    capped = limit < PROVIDER_SEARCH_LIMIT_CAP ? limit : (double)PROVIDER_SEARCH_LIMIT_CAP;

    json_set(o, "mode", w25_provider_f2f12e_resolve_search_mode(req));
    json_set(o, "max_results", json_number(capped));
    json_append(list, query ? json_copy(query) : json_null());
    json_set(o, "search_queries", list);
    json_set(o, "objective", query ? json_copy(query) : json_null());
    return o;
}

/* " ".join(excerpts or []): strings joined by one space; falsy/non-list -> "". */
static json_t *pv_description(const json_t *excerpts) {
    json_t *out;
    size_t i, n;
    size_t len = 1;
    char *buf, *w;
    if (!excerpts || excerpts->type != JSON_ARRAY || json_len(excerpts) == 0) return json_string("");
    n = json_len(excerpts);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(excerpts, i);
        if (e && e->type == JSON_STRING && e->str_val) len += strlen(e->str_val);
        if (i > 0) len += 1;
    }
    buf = (char *)malloc(len);
    if (!buf) return json_string("");
    w = buf;
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(excerpts, i);
        const char *s = (e && e->type == JSON_STRING && e->str_val) ? e->str_val : "";
        size_t l = strlen(s);
        if (i > 0) *w++ = ' ';
        memcpy(w, s, l);
        w += l;
    }
    *w = '\0';
    out = json_string(buf);
    free(buf);
    return out;
}

/* r.url or "": the value itself when truthy, else the empty string. */
static json_t *pv_or_empty(const json_t *v) {
    if (pv_truthy(v)) return json_copy(v);
    return json_string("");
}

json_t *w25_provider_f2f12e_search_hits(const json_t *req) {
    const json_t *results = req ? json_obj_get(req, "results") : NULL;
    json_t *web = json_array(), *data = json_object(), *o = json_object();
    size_t i, n = (results && results->type == JSON_ARRAY) ? json_len(results) : 0;

    for (i = 0; i < n; i++) {
        const json_t *r = json_get(results, i);
        json_t *hit;
        if (!r || r->type != JSON_OBJECT) continue;
        hit = json_object();
        json_set(hit, "url", pv_or_empty(json_obj_get(r, "url")));
        json_set(hit, "title", pv_or_empty(json_obj_get(r, "title")));
        json_set(hit, "description", pv_description(json_obj_get(r, "excerpts")));
        json_set(hit, "position", json_number((double)(i + 1)));
        json_append(web, hit);
    }
    json_set(data, "web", web);
    json_set(o, "success", json_bool(1));
    json_set(o, "data", data);
    return o;
}

json_t *w25_provider_f2f12e_constants(void) {
    json_t *o = json_object(), *modes = json_array();
    json_set(o, "name", json_string("parallel"));
    json_set(o, "display_name", json_string("Parallel"));
    json_set(o, "key_env", json_string("PARALLEL_API_KEY"));
    json_set(o, "extract", json_bool(1));
    json_set(o, "keyless", json_bool(1));
    json_set(o, "search_limit_cap", json_number((double)PROVIDER_SEARCH_LIMIT_CAP));
    json_set(o, "missing_key", json_string("PARALLEL_API_KEY environment variable not set. "
                                           "Get your API key at https://parallel.ai"));
    json_append(modes, json_string("agentic"));
    json_append(modes, json_string("fast"));
    json_append(modes, json_string("one-shot"));
    json_set(o, "search_modes", modes);
    return o;
}
