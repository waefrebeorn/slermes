/*
 * provider.c - C11 port of upstream plugins/web/tavily/provider.py
 * (gate w24_provider_4aa510).
 *
 * PoP: w24_provider_4aa510_headers @ plugins/web/tavily/provider.py:_tavily_headers
 * PoP: w24_provider_4aa510_search_payload @ plugins/web/tavily/provider.py:TavilyWebSearchProvider.search
 * PoP: w24_provider_4aa510_normalize_search @ plugins/web/tavily/provider.py:_normalize_tavily_search_results
 * PoP: w24_provider_4aa510_normalize_documents @ plugins/web/tavily/provider.py:_normalize_tavily_documents
 * PoP: w24_provider_4aa510_missing_key_error @ plugins/web/tavily/provider.py:_missing_key_error
 * PoP: w24_provider_4aa510_auth @ plugins/web/tavily/provider.py:_auth
 * PoP: w24_provider_4aa510_setup_schema @ plugins/web/tavily/provider.py:get_setup_schema
 * PoP: w24_provider_4aa510_constants @ plugins/web/tavily/provider.py:<provider class literals>
 *
 * What is ported is everything the provider decides before or after the HTTP
 * call: which headers a keyed vs keyless call carries, the /search payload
 * (clamped to the vendor's 20-result cap, keeping the caller's operand type as
 * Python's min does), the two normalizers that produce the shared
 * search/extract shapes in their contract key order, the missing-key text, the
 * auth triple, and the `hermes tools` entry.  httpx.post is the seam -- it is
 * not ported; ``_auth``'s env reads arrive as the api_key argument and the
 * keyless flag.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_provider_4aa510.h"

#define TV_CLIENT_NAME "hermes-agent"
#define TV_NAME "tavily"
#define TV_DISPLAY_NAME "Tavily"
#define TV_KEY_ENV "TAVILY_API_KEY"
#define TV_LIMIT_CAP 20

/* --- Python value helpers ------------------------------------------------- */
typedef struct { char *b; size_t n, cap; int oom; } tv_buf;

static void tv_bput(tv_buf *s, const char *t, size_t len) {
    if (s->oom) return;
    if (s->n + len + 1 > s->cap) {
        size_t cap = s->cap ? s->cap : 64;
        while (cap < s->n + len + 1) cap *= 2;
        char *nb = (char *)realloc(s->b, cap);
        if (!nb) { s->oom = 1; return; }
        s->b = nb;
        s->cap = cap;
    }
    memcpy(s->b + s->n, t, len);
    s->n += len;
    s->b[s->n] = '\0';
}

static void tv_bputs(tv_buf *s, const char *t) { tv_bput(s, t, strlen(t)); }

static char *tv_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* True when Python's min() can compare and return this value. */
static int tv_comparable(const json_t *v, double *num) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *num = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *num = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python truthiness. */
static int tv_truthy(const json_t *v) {
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

static void tv_num_text(double v, char *out, size_t cap) {
    char tmp[64];
    int p;
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) {
        snprintf(out, cap, "%.0f", v);
        return;
    }
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) { snprintf(out, cap, "%s", tmp); return; }
    }
    snprintf(out, cap, "%.17g", v);
}

static void tv_repr(tv_buf *s, const json_t *v);

static void tv_repr_str(tv_buf *s, const char *t) {
    tv_bput(s, "'", 1);
    for (const char *p = t; *p; p++) {
        if (*p == '\'') tv_bputs(s, "\\'");
        else if (*p == '\\') tv_bputs(s, "\\\\");
        else if (*p == '\n') tv_bputs(s, "\\n");
        else if (*p == '\t') tv_bputs(s, "\\t");
        else tv_bput(s, p, 1);
    }
    tv_bput(s, "'", 1);
}

static void tv_repr(tv_buf *s, const json_t *v) {
    size_t i, n;
    char num[64];
    if (!v) { tv_bputs(s, "None"); return; }
    switch (v->type) {
    case JSON_NULL: tv_bputs(s, "None"); return;
    case JSON_BOOL: tv_bputs(s, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: tv_num_text(v->num_val, num, sizeof(num)); tv_bputs(s, num); return;
    case JSON_STRING: tv_repr_str(s, v->str_val ? v->str_val : ""); return;
    case JSON_ARRAY:
        tv_bput(s, "[", 1);
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) tv_bputs(s, ", ");
            tv_repr(s, json_get(v, i));
        }
        tv_bput(s, "]", 1);
        return;
    case JSON_OBJECT:
        tv_bput(s, "{", 1);
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            if (i) tv_bputs(s, ", ");
            tv_repr_str(s, json_object_get_key_at(v, i));
            tv_bputs(s, ": ");
            tv_repr(s, json_object_get_at(v, i));
        }
        tv_bput(s, "}", 1);
        return;
    }
}

/* str(v). Caller frees. */
static char *tv_pystr(const json_t *v) {
    tv_buf s = {NULL, 0, 0, 0};
    if (v && v->type == JSON_STRING) return tv_dup(v->str_val ? v->str_val : "");
    tv_repr(&s, v);
    if (s.oom) { free(s.b); return NULL; }
    if (!s.b) return tv_dup("");
    return s.b;
}

static json_t *tv_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

/* r.get(key, "") as an owned node. */
static json_t *tv_field(const json_t *o, const char *key) {
    const json_t *v = json_obj_get(o, key);
    return v ? json_copy(v) : json_string("");
}

/* The exception a non-list `results` raises in enumerate()/iteration. */
static const char *tv_iter_error(const json_t *v) {
    if (!v) return "TypeError";              /* iterate None */
    switch (v->type) {
    case JSON_STRING: return "AttributeError";  /* each char has no .get */
    case JSON_OBJECT: return "AttributeError";  /* each key is a str */
    case JSON_ARRAY:  return NULL;
    default:          return "TypeError";       /* int / bool / null */
    }
}

json_t *w24_provider_4aa510_headers(const json_t *api_key) {
    json_t *o = json_object();
    json_set(o, "X-Client-Name", json_string(TV_CLIENT_NAME));
    if (tv_truthy(api_key)) {
        char *k = tv_pystr(api_key);
        size_t need = strlen(k ? k : "") + 8;
        char *v = (char *)malloc(need);
        if (v) snprintf(v, need, "Bearer %s", k ? k : "");
        json_set(o, "Authorization", json_string(v ? v : "Bearer "));
        free(v);
        free(k);
    } else {
        json_set(o, "X-Tavily-Access-Mode", json_string("keyless"));
    }
    return o;
}

json_t *w24_provider_4aa510_search_payload(const json_t *query, const json_t *limit) {
    json_t *o;
    double n = 0.0;
    if (!tv_comparable(limit, &n)) return tv_error("TypeError");
    o = json_object();
    json_set(o, "query", query ? json_copy(query) : json_null());
    /* min(limit, SEARCH_LIMIT_CAP) keeps the caller's operand when it is smaller
     * (so a bool stays a bool, as min does). */
    json_set(o, "max_results", n < (double)TV_LIMIT_CAP ? json_copy(limit) : json_number(TV_LIMIT_CAP));
    json_set(o, "include_raw_content", json_bool(0));
    json_set(o, "include_images", json_bool(0));
    return o;
}

json_t *w24_provider_4aa510_normalize_search(const json_t *response) {
    const json_t *results;
    const char *err;
    json_t *web, *out, *data;
    size_t i, n;

    if (!response || response->type != JSON_OBJECT) return tv_error("AttributeError");
    results = json_obj_get(response, "results");
    if (!results) {
        web = json_array();
    } else {
        err = tv_iter_error(results);
        if (err) return tv_error(err);
        web = json_array();
        n = json_len(results);
        for (i = 0; i < n; i++) {
            const json_t *r = json_get(results, i);
            json_t *hit;
            if (!r || r->type != JSON_OBJECT) {   /* r.get on a non-dict */
                json_free(web);
                return tv_error("AttributeError");
            }
            /* title_hit(r.get("title",""), r.get("url",""), r.get("content",""), i+1) */
            hit = json_object();
            json_set(hit, "title", tv_field(r, "title"));
            json_set(hit, "url", tv_field(r, "url"));
            json_set(hit, "description", tv_field(r, "content"));
            json_set(hit, "position", json_number((double)(i + 1)));
            json_append(web, hit);
        }
    }
    /* search_ok(web) */
    out = json_object();
    json_set(out, "success", json_bool(1));
    data = json_object();
    json_set(data, "web", web);
    json_set(out, "data", data);
    return out;
}

/* document(url, title, content) */
static json_t *tv_document(const json_t *url, const json_t *title, const json_t *content) {
    json_t *doc = json_object();
    json_t *meta = json_object();
    json_set(doc, "url", url ? json_copy(url) : json_null());
    json_set(doc, "title", title ? json_copy(title) : json_null());
    json_set(doc, "content", content ? json_copy(content) : json_null());
    json_set(doc, "raw_content", content ? json_copy(content) : json_null());
    json_set(meta, "sourceURL", url ? json_copy(url) : json_null());
    json_set(meta, "title", title ? json_copy(title) : json_null());
    json_set(doc, "metadata", meta);
    return doc;
}

/* _failed_document(url, error) */
static json_t *tv_failed(const json_t *url, const json_t *error) {
    json_t *doc = json_object();
    json_t *meta = json_object();
    json_set(doc, "url", url ? json_copy(url) : json_null());
    json_set(doc, "title", json_string(""));
    json_set(doc, "content", json_string(""));
    json_set(doc, "raw_content", json_string(""));
    json_set(doc, "error", error ? json_copy(error) : json_null());
    json_set(meta, "sourceURL", url ? json_copy(url) : json_null());
    json_set(doc, "metadata", meta);
    return doc;
}

json_t *w24_provider_4aa510_normalize_documents(const json_t *response) {
    const json_t *results, *failed, *failed_urls;
    const char *err;
    json_t *docs;
    size_t i, n;

    if (!response || response->type != JSON_OBJECT) return tv_error("AttributeError");
    results = json_obj_get(response, "results");
    failed = json_obj_get(response, "failed_results");
    failed_urls = json_obj_get(response, "failed_urls");
    docs = json_array();

    if (results) {
        err = tv_iter_error(results);
        if (err) { json_free(docs); return tv_error(err); }
        n = json_len(results);
        for (i = 0; i < n; i++) {
            const json_t *r = json_get(results, i);
            const json_t *raw, *content;
            if (!r || r->type != JSON_OBJECT) {
                json_free(docs);
                return tv_error("AttributeError");
            }
            /* r.get("raw_content","") or r.get("content","") */
            raw = json_obj_get(r, "raw_content");
            if (!raw) raw = json_string("");
            content = tv_truthy(raw) ? raw
                                     : (json_obj_get(r, "content") ? json_obj_get(r, "content") : json_string(""));
            json_append(docs, tv_document(tv_field(r, "url"), tv_field(r, "title"), content));
        }
    }
    if (failed) {
        err = tv_iter_error(failed);
        if (err) { json_free(docs); return tv_error(err); }
        n = json_len(failed);
        for (i = 0; i < n; i++) {
            const json_t *f = json_get(failed, i);
            if (!f || f->type != JSON_OBJECT) {
                json_free(docs);
                return tv_error("AttributeError");
            }
            json_append(docs, tv_failed(json_obj_get(f, "url") ? json_obj_get(f, "url") : json_string(""),
                                        json_obj_get(f, "error") ? json_obj_get(f, "error")
                                                                 : json_string("extraction failed")));
        }
    }
    if (failed_urls) {
        err = tv_iter_error(failed_urls);
        if (err) { json_free(docs); return tv_error(err); }
        n = json_len(failed_urls);
        for (i = 0; i < n; i++) {
            char *text = tv_pystr(json_get(failed_urls, i));   /* str(u) */
            json_append(docs, tv_failed(json_string(text ? text : ""), json_string("extraction failed")));
            free(text);
        }
    }
    return docs;
}

json_t *w24_provider_4aa510_missing_key_error(const json_t *action) {
    char *a = tv_pystr(action);
    size_t need = strlen(a ? a : "") + 256;
    char *buf = (char *)malloc(need);
    json_t *out;
    if (!buf) { free(a); return json_string(""); }
    snprintf(buf, need,
             "TAVILY_API_KEY is not set. Get a key at https://app.tavily.com/home "
             "or select Tavily in `hermes tools` for opt-in keyless %s.",
             a ? a : "");
    out = json_string(buf);
    free(buf);
    free(a);
    return out;
}

json_t *w24_provider_4aa510_auth(const json_t *api_key, const json_t *keyless,
                                 const json_t *action) {
    json_t *out = json_object();
    int force_keyless = tv_truthy(keyless);

    if (!force_keyless && !tv_truthy(api_key)) {
        json_set(out, "key", json_null());
        json_set(out, "missing", w24_provider_4aa510_missing_key_error(action));
        json_set(out, "prefix", json_string(""));
        return out;
    }
    if (force_keyless) {
        json_set(out, "key", json_string(""));      /* "" forces the keyless header */
        json_set(out, "missing", json_null());
        json_set(out, "prefix", json_string("keyless "));
        return out;
    }
    json_set(out, "key", json_copy(api_key));
    json_set(out, "missing", json_null());
    json_set(out, "prefix", json_string(""));
    return out;
}

json_t *w24_provider_4aa510_setup_schema(void) {
    json_t *o = json_object();
    json_t *vars = json_array();
    json_t *entry = json_object();
    json_set(entry, "key", json_string(TV_KEY_ENV));
    json_set(entry, "prompt", json_string("Tavily API key (optional — keyless works when Tavily is selected)"));
    json_set(entry, "url", json_string("https://app.tavily.com/home"));
    json_append(vars, entry);
    json_set(o, "name", json_string(TV_DISPLAY_NAME));
    json_set(o, "badge", json_string("free · key optional"));
    json_set(o, "tag", json_string("Search + extract. Opt-in keyless; set TAVILY_API_KEY for higher limits."));
    json_set(o, "env_vars", vars);
    return o;
}

json_t *w24_provider_4aa510_constants(void) {
    json_t *o = json_object();
    json_t *payload = json_object();
    json_set(payload, "include_raw_content", json_bool(0));
    json_set(payload, "include_images", json_bool(0));
    json_set(o, "NAME", json_string(TV_NAME));
    json_set(o, "DISPLAY_NAME", json_string(TV_DISPLAY_NAME));
    json_set(o, "KEY_ENV", json_string(TV_KEY_ENV));
    json_set(o, "EXTRACT", json_bool(1));
    json_set(o, "KEYLESS", json_bool(1));
    json_set(o, "_CLIENT_NAME", json_string(TV_CLIENT_NAME));
    json_set(o, "_SEARCH_PAYLOAD", payload);
    json_set(o, "SEARCH_LIMIT_CAP", json_number(TV_LIMIT_CAP));
    return o;
}
