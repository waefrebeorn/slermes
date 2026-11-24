/*
 * plugins/web/exa/provider.c -- real C11 port of the request/response shaping
 * and keyless/tier decision of upstream Hermes plugins/web/exa/provider.py.
 *
 * PoP: w20_provider_8219de_search @ plugins/web/exa/provider.py:ExaWebSearchProvider.search
 * PoP: w20_provider_8219de_extract @ plugins/web/exa/provider.py:ExaWebSearchProvider.extract
 * PoP: w20_provider_8219de_crawl_status @ plugins/web/exa/provider.py:_crawl_status
 * PoP: w20_provider_8219de_vendor_error_entry @ plugins/web/_common.py:vendor_error_entry
 * PoP: w20_provider_8219de_document @ plugins/web/_common.py:document
 * PoP: w20_provider_8219de_setup_schema @ plugins/web/exa/provider.py:ExaWebSearchProvider.get_setup_schema
 * PoP: w20_provider_8219de_use_keyless @ plugins/web/keyless_mcp.py:use_keyless
 * PoP: w20_provider_8219de_missing_key @ plugins/web/exa/provider.py:_MISSING_KEY
 *
 * The exa-py SDK call, the keyless MCP ring and the env lookup stay upstream;
 * everything here is a pure function of the SDK's decoded answer.
 */
#include "hermes_json.h"
#include "noc_w20_provider_8219de.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *exap_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int exap_is_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d)) return 0;
    return floor(d) == d;
}

/* Python `str(v)` for the scalar shapes a vendor error can carry. */
static char *exap_str(const json_t *v) {
    char buf[64];
    if (!v) return exap_dup("");
    switch (v->type) {
    case JSON_NULL:   return exap_dup("None");
    case JSON_BOOL:   return exap_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return exap_dup(v->str_val);
    case JSON_NUMBER:
        if (exap_is_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return exap_dup(buf);
    case JSON_ARRAY:  return exap_dup("[]");
    case JSON_OBJECT: return exap_dup("{}");
    }
    return exap_dup("");
}

static int exap_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `v or default` for scalars: returns the value when truthy, else default. */
static char *exap_or(const json_t *v, const char *def) {
    if (exap_truthy(v)) return exap_str(v);
    return exap_dup(def);
}

/* Python int(v) -> 1 on success, 0 when it would raise. */
static int exap_py_int(const json_t *v, long long *out) {
    const char *s;
    char *end;
    long long n;
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 0;             /* isinstance(status, bool) short-circuits */
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return 0;
        *out = (long long)v->num_val;
        return 1;
    }
    if (v->type != JSON_STRING) return 0;
    s = v->str_val;
    while (*s && isspace((unsigned char)*s)) s++;
    n = strtoll(s, &end, 10);
    if (end == s) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) return 0;
    *out = n;
    return 1;
}

/* origin_http_error(status, reason) */
static char *exap_origin_http_error(const json_t *status, const char *reason) {
    long long code;
    char buf[64];
    char *detail;
    if (!status || status->type == JSON_BOOL) return NULL;
    if (!exap_py_int(status, &code)) return NULL;
    if (code < 400) return NULL;
    detail = reason ? exap_dup(reason) : NULL;
    if (detail) {
        size_t n = strlen(detail);
        while (n > 0 && isspace((unsigned char)detail[n - 1])) detail[--n] = '\0';
        {
            char *p = detail;
            while (*p && isspace((unsigned char)*p)) p++;
            if (p != detail) memmove(detail, p, strlen(p) + 1);
        }
    }
    if (detail && detail[0]) {
        snprintf(buf, sizeof(buf), "HTTP %lld from origin: %s", code, detail);
    } else {
        snprintf(buf, sizeof(buf), "HTTP %lld from origin", code);
    }
    free(detail);
    return exap_dup(buf);
}

/* ---- _EXA_CRAWL_STATUS_RE = \bCRAWL_(?:HTTP_(\d{3})|(NOT_FOUND))\s*$ ---- */

static int exap_is_word(unsigned char c) {
    return isalnum(c) || c == '_';
}

static json_t *exap_crawl_tag_status(const json_t *tag) {
    char *s = exap_str(tag);          /* str(text or "") */
    size_t n, i;
    const char *tail = NULL;
    json_t *out = json_null();
    if (!s) return json_null();
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';  /* \s*$ */
    for (i = n; i-- > 0;) {
        if (i + 6 <= n && !strncmp(s + i, "CRAWL_", 6)) { tail = s + i; break; }
    }
    if (tail) {
        const char *rest = tail + 6;
        int bounded = (tail == s) || !exap_is_word((unsigned char)tail[-1]);
        if (bounded) {
            if (!strcmp(rest, "NOT_FOUND")) {
                json_free(out);
                out = json_number(404);
            } else if (!strncmp(rest, "HTTP_", 5) && strlen(rest) == 8 &&
                       isdigit((unsigned char)rest[5]) && isdigit((unsigned char)rest[6]) &&
                       isdigit((unsigned char)rest[7])) {
                json_free(out);
                out = json_number((double)((rest[5] - '0') * 100 + (rest[6] - '0') * 10 +
                                           (rest[7] - '0')));
            }
        }
    }
    free(s);
    return out;
}

/* exa_crawl_status(text) */
static json_t *exap_exa_crawl_status(const json_t *tag) {
    return exap_crawl_tag_status(tag);
}

json_t *w20_provider_8219de_crawl_status(const json_t *error) {
    json_t *code, *tag, *out;
    if (!error || error->type != JSON_OBJECT) return exap_crawl_tag_status(NULL);
    code = json_obj_get(error, "http_status_code");
    if (exap_truthy(code)) {
        out = json_copy(code);
        return out;
    }
    tag = json_obj_get(error, "tag");
    return exap_exa_crawl_status(tag);
}

/* ---- result rows ---- */

/* `x or ""` as a JSON value, keeping the original type when x is truthy. */
static json_t *exap_or_empty_j(const json_t *v) {
    if (!exap_truthy(v)) return json_string("");
    return json_copy(v);
}

static json_t *exap_document_j(const json_t *url, const json_t *title, const json_t *content) {
    json_t *out = json_object(), *meta = json_object();
    json_t *u = exap_or_empty_j(url), *t = exap_or_empty_j(title), *c = exap_or_empty_j(content);
    json_set(out, "url", u);
    json_set(out, "title", t);
    json_set(out, "content", json_copy(c));
    json_set(out, "raw_content", c);
    json_set(meta, "sourceURL", json_copy(u));
    json_set(meta, "title", json_copy(t));
    json_set(out, "metadata", meta);
    return out;
}

json_t *w20_provider_8219de_document(const char *url, const char *title, const char *content) {
    json_t *out = exap_document_j(json_string(url ? url : ""), json_string(title ? title : ""),
                                  json_string(content ? content : ""));
    return out;
}

/* page_error(url, error) with the URL kept as given by the vendor. */
static json_t *exap_page_error_j(const json_t *url, const char *error) {
    json_t *out = json_object();
    json_set(out, "url", exap_or_empty_j(url));
    json_set(out, "title", json_string(""));
    json_set(out, "content", json_string(""));
    json_set(out, "error", json_string(error ? error : ""));
    return out;
}

json_t *w20_provider_8219de_vendor_error_entry(const json_t *url, const json_t *status,
                                               const json_t *reason, const json_t *body) {
    char *origin = exap_origin_http_error(status, NULL);
    json_t *entry, *meta;
    if (origin) {
        char *content = exap_truthy(body) ? exap_str(body) : exap_dup("");
        entry = exap_page_error_j(url, origin);
        json_set(entry, "content", json_string(content));
        free(content);
        free(origin);
    } else {
        char *text;
        if (exap_truthy(body)) text = exap_str(body);
        else if (exap_truthy(reason)) text = exap_str(reason);
        else text = exap_dup("extraction failed");
        entry = exap_page_error_j(url, text);
        free(text);
    }
    meta = json_object();
    json_set(meta, "sourceURL", exap_or_empty_j(url));
    json_set(entry, "metadata", meta);
    return entry;
}

/* " ".join(v or []) for a string array; `or ""` for missing / non-string fields. */
static char *exap_join_highlights(const json_t *highlights) {
    size_t i, total = 0;
    char *out;
    if (!highlights || highlights->type != JSON_ARRAY || highlights->c.count == 0) return exap_dup("");
    for (i = 0; i < highlights->c.count; i++) {
        json_t *h = highlights->c.items[i];
        if (h && h->type == JSON_STRING && h->str_val) total += strlen(h->str_val) + 1;
    }
    out = (char *)malloc(total + 1);
    if (!out) return exap_dup("");
    out[0] = '\0';
    for (i = 0; i < highlights->c.count; i++) {
        json_t *h = highlights->c.items[i];
        if (!h || h->type != JSON_STRING || !h->str_val) continue;
        if (out[0]) strcat(out, " ");
        strcat(out, h->str_val);
    }
    return out;
}

json_t *w20_provider_8219de_search(const json_t *results) {
    json_t *web = json_array();
    json_t *out = json_object(), *data = json_object();
    size_t i;
    for (i = 0; results && results->type == JSON_ARRAY && i < results->c.count; i++) {
        json_t *r = results->c.items[i];
        json_t *hit = json_object();
        char *desc = exap_join_highlights(json_obj_get(r, "highlights"));
        json_set(hit, "url", exap_or_empty_j(json_obj_get(r, "url")));
        json_set(hit, "title", exap_or_empty_j(json_obj_get(r, "title")));
        json_set(hit, "description", json_string(desc ? desc : ""));
        json_set(hit, "position", json_number((double)(i + 1)));
        free(desc);
        json_append(web, hit);
    }
    json_set(data, "web", web);
    json_set(out, "success", json_bool(1));
    json_set(out, "data", data);
    return out;
}

json_t *w20_provider_8219de_extract(const json_t *results, const json_t *statuses) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; results && results->type == JSON_ARRAY && i < results->c.count; i++) {
        json_t *r = results->c.items[i];
        json_append(out, exap_document_j(json_obj_get(r, "url"), json_obj_get(r, "title"),
                                         json_obj_get(r, "text")));
    }
    for (i = 0; statuses && statuses->type == JSON_ARRAY && i < statuses->c.count; i++) {
        json_t *s = statuses->c.items[i];
        json_t *err, *sid, *reason, *status_v;
        char *id_s;
        json_t *st;
        if (!s || s->type != JSON_OBJECT) continue;
        status_v = json_obj_get(s, "status");
        /* `s.status == "error"` -- only a real string matches. */
        if (!status_v || status_v->type != JSON_STRING || strcmp(status_v->str_val, "error")) continue;
        sid = json_obj_get(s, "id");
        err = json_obj_get(s, "error");
        reason = (err && err->type == JSON_OBJECT) ? json_obj_get(err, "tag") : NULL;
        id_s = exap_or(sid, "");
        st = w20_provider_8219de_crawl_status(err);
        json_append(out, w20_provider_8219de_vendor_error_entry(json_string(id_s), st, reason, NULL));
        free(id_s);
        json_free(st);
    }
    return out;
}

/* ---- picker entry ---- */

static json_t *exap_setup_schema(const char *name, const char *badge, const char *tag,
                                 const char *key_env, const char *prompt, const char *url,
                                 const char *web_tier) {
    json_t *out = json_object();
    json_set(out, "name", json_string(name));
    json_set(out, "badge", json_string(badge));
    json_set(out, "tag", json_string(tag));
    if (key_env && key_env[0]) {
        json_t *env = json_array(), *item = json_object();
        json_set(item, "key", json_string(key_env));
        json_set(item, "prompt", json_string(prompt ? prompt : ""));
        json_set(item, "url", json_string(url ? url : ""));
        json_append(env, item);
        json_set(out, "env_vars", env);
    } else {
        json_set(out, "env_vars", json_array());
    }
    json_set(out, "web_tier", json_string(web_tier ? web_tier : ""));
    return out;
}

json_t *w20_provider_8219de_setup_schema(void) {
    json_t *paid, *out, *variants;
    const char *free_tag =
        "Semantic + neural web search with content extraction on Exa's anonymous free tier. "
        "Rate-limited under burst load.";
    const char *paid_tag =
        "Semantic + neural web search with content extraction via the Exa SDK. Unthrottled, "
        "guaranteed service.";
    /* "Exa <U+00B7> Paid (API key)" -- the middle-dot separator, ASCII source. */
    paid = exap_setup_schema("Exa \xC2\xB7 Paid (API key)", "paid", paid_tag, "EXA_API_KEY",
                             "Exa API key", "https://exa.ai", "paid");
    out = exap_setup_schema("Exa \xC2\xB7 Free (keyless)", "free \xC2\xB7 no key", free_tag,
                            "", "", "", "free");
    variants = json_array();
    json_append(variants, paid);
    json_set(out, "variants", variants);
    return out;
}

int w20_provider_8219de_use_keyless(const json_t *tier, const char *api_key, int enabled) {
    const char *t = (tier && tier->type == JSON_STRING) ? tier->str_val : NULL;
    if (t && (!strcmp(t, "free") || !strcmp(t, "paid"))) return !strcmp(t, "free");
    return (!api_key || !api_key[0]) && enabled;
}

const char *w20_provider_8219de_missing_key(void) {
    return "EXA_API_KEY environment variable not set. Get your API key at https://exa.ai";
}
