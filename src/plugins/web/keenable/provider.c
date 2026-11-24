/*
 * provider.c -- real C11 port of the Keenable web search provider in upstream
 * Hermes plugins/web/keenable/provider.py.
 *
 * PoP: w20_provider_9c3993_headers @ plugins/web/keenable/provider.py:_keenable_headers
 * PoP: w20_provider_9c3993_search_request @ plugins/web/keenable/provider.py:KeenableWebSearchProvider.search
 * PoP: w20_provider_9c3993_search_result @ plugins/web/keenable/provider.py:KeenableWebSearchProvider.search
 * PoP: w20_provider_9c3993_extract @ plugins/web/keenable/provider.py:KeenableWebSearchProvider.extract
 * PoP: w20_provider_9c3993_setup_schema @ plugins/web/keenable/provider.py:KeenableWebSearchProvider.get_setup_schema
 * PoP: w20_provider_9c3993_identity @ plugins/web/keenable/provider.py:KeenableWebSearchProvider
 *
 * requests is a seam: the port builds the keyed POST the provider would send
 * and maps a scripted reply through _common's result shapes, including the
 * Keenable origin-error translation (target status vs Keenable's own refusal).
 */
#include "hermes_json.h"
#include "noc_w20_provider_9c3993.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KE_API_URL   "https://api.keenable.ai"
#define KE_LIMIT_CAP 20

static const char *const KE_FREE_TAG =
    "Independent web index for AI apps \xE2\x80\x94 fast search + page fetch on "
    "Keenable's anonymous free tier.";

static const char *const KE_PAID_TAG =
    "Independent web index for AI apps. Keyed access with higher limits and guaranteed service.";

static int ke_truthy(const json_t *v) {
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

static const json_t *ke_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

static const char *ke_text(const json_t *obj, const char *key, const char *def) {
    const json_t *v = ke_get(obj, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static int ke_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

/* `value or ""` keeping the decoded value's type (numbers stay numbers). */
static json_t *ke_or_empty(const json_t *v) {
    return ke_truthy(v) ? json_copy(v) : json_string("");
}

/* str.strip() into a static-free copy the caller frees. */
static char *ke_strip(const char *s) {
    const char *start = s ? s : "";
    const char *end;
    char *out;
    while (*start && ke_space(*start)) start++;
    end = start + strlen(start);
    while (end > start && ke_space(end[-1])) end--;
    out = (char *)malloc((size_t)(end - start) + 1);
    if (!out) return NULL;
    memcpy(out, start, (size_t)(end - start));
    out[end - start] = '\0';
    return out;
}

static char *ke_concat(const char *a, const char *b) {
    size_t na = a ? strlen(a) : 0, nb = b ? strlen(b) : 0;
    char *out = (char *)malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

static json_t *ke_search_fail(const char *error) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(error ? error : ""));
    return o;
}

static json_t *ke_web_hit(json_t *url, json_t *title, json_t *description, size_t position) {
    json_t *o = json_object();
    json_set(o, "url", url);
    json_set(o, "title", title);
    json_set(o, "description", description);
    json_set(o, "position", json_number((double)position));
    return o;
}

static json_t *ke_page_error(const char *url, const char *error) {
    json_t *o = json_object();
    json_set(o, "url", json_string(url ? url : ""));
    json_set(o, "title", json_string(""));
    json_set(o, "content", json_string(""));
    json_set(o, "error", json_string(error ? error : ""));
    return o;
}

static json_t *ke_document(json_t *url, json_t *title, json_t *content, const char *source_url) {
    json_t *o = json_object();
    json_t *meta = json_object();
    json_set(meta, "sourceURL", json_string(source_url ? source_url : ""));
    json_set(meta, "title", json_copy(title));
    json_set(o, "url", url);
    json_set(o, "title", title);
    json_set(o, "content", content);
    json_set(o, "raw_content", json_copy(content));
    json_set(o, "metadata", meta);
    return o;
}

/* _common.http_status_detail: the body text, else "HTTP <code>". */
static char *ke_status_detail(int status, const char *text) {
    char *detail = ke_strip(text);
    if (detail && detail[0]) return detail;
    free(detail);
    detail = (char *)malloc(32);
    if (!detail) return NULL;
    snprintf(detail, 32, "HTTP %d", status);
    return detail;
}

/* origin_http_error: "HTTP <code> from origin[: reason]" for a >=400 target. */
static char *ke_origin_http(int status, const char *reason) {
    char *detail, *out;
    size_t n;
    if (status < 400) return NULL;
    detail = ke_strip(reason);
    n = strlen(detail ? detail : "") + 64;
    out = (char *)malloc(n);
    if (out) {
        if (detail && detail[0])
            snprintf(out, n, "HTTP %d from origin: %s", status, detail);
        else
            snprintf(out, n, "HTTP %d from origin", status);
    }
    free(detail);
    return out;
}

/* `message` str() for the origin-message check. */
static char *ke_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return ke_concat("", "");
    if (v->type == JSON_STRING) return ke_concat(v->str_val ? v->str_val : "", "");
    if (v->type == JSON_BOOL) return ke_concat(v->bool_val ? "True" : "False", "");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return ke_concat(buf, "");
    }
    return ke_concat("", "");
}

/* _KEENABLE_ORIGIN_MESSAGE_RE = r"the (?:target server|page does not exist)\b" */
static int ke_prefix_ci(const char *text, const char *word) {
    size_t i;
    for (i = 0; word[i]; i++) {
        char a = text[i], b = word[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int ke_word_boundary(char c) {
    return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') || c == '_');
}

static char *ke_origin_error(int status, const char *body) {
    json_t *data;
    char *message, *out = NULL;
    if (!body || !body[0]) return NULL;
    data = json_parse(body, NULL);
    if (!data) return NULL;                    /* json.loads raised -> None */
    if (data->type == JSON_OBJECT) {
        message = ke_pystr(ke_get(data, "message"));
    } else {
        message = ke_concat("", "");
    }
    {
        char *stripped = ke_strip(message);
        free(message);
        if (stripped && stripped[0]) {
            int hit = 0;
            if (ke_prefix_ci(stripped, "the target server"))
                hit = ke_word_boundary(stripped[strlen("the target server")]);
            else if (ke_prefix_ci(stripped, "the page does not exist"))
                hit = ke_word_boundary(stripped[strlen("the page does not exist")]);
            if (hit) out = ke_origin_http(status, stripped);
        }
        free(stripped);
    }
    json_free(data);
    return out;
}

json_t *w20_provider_9c3993_headers(const char *api_key) {
    json_t *o = json_object();
    json_set(o, "X-Keenable-Title", json_string("hermes-agent"));
    if (api_key && api_key[0]) {
        char *value = ke_concat("Bearer ", api_key);
        json_set(o, "Authorization", json_string(value ? value : ""));
        free(value);
    }
    return o;
}

json_t *w20_provider_9c3993_search_request(const char *query, double limit, int keyless,
                                            const char *api_key) {
    json_t *o = json_object();
    json_set(o, "keyless", json_bool(keyless ? true : false));
    if (!keyless) {
        long long n = (long long)limit;
        json_t *body = json_object();
        char *url = ke_concat(KE_API_URL, "/v1/search");
        if (n < 1) n = 1;
        if (n > KE_LIMIT_CAP) n = KE_LIMIT_CAP;
        json_set(body, "query", json_string(query ? query : ""));
        json_set(body, "max_results", json_number((double)n));
        json_set(o, "url", json_string(url ? url : ""));
        json_set(o, "body", body);
        json_set(o, "headers", w20_provider_9c3993_headers(api_key));
        free(url);
    }
    return o;
}

json_t *w20_provider_9c3993_search_result(const json_t *reply) {
    int status = (int)json_get_num(reply, "status", 0);
    const char *text = ke_text(reply, "text", "");
    const char *json_error = ke_text(reply, "json_error", "");
    const json_t *body = ke_get(reply, "body");
    const json_t *rows;
    json_t *o, *web;
    size_t i, n;

    if (status >= 400) {
        char *detail = ke_status_detail(status, text);
        char *msg = ke_concat("Keenable search failed: ", detail ? detail : "");
        o = ke_search_fail(msg);
        free(detail);
        free(msg);
        return o;
    }
    if (json_error[0]) {
        char *msg = ke_concat("Keenable search failed: ", json_error);
        o = ke_search_fail(msg);
        free(msg);
        return o;
    }
    rows = ke_get(body, "results");
    if (!rows || rows->type != JSON_ARRAY) rows = NULL;
    web = json_array();
    n = rows ? rows->c.count : 0;
    for (i = 0; i < n; i++) {
        json_t *row = rows->c.items[i];
        json_t *snippet = ke_get(row, "snippet");
        json_t *desc = ke_truthy(snippet) ? snippet : ke_get(row, "description");
        json_append(web, ke_web_hit(ke_or_empty(ke_get(row, "url")),
                                    ke_or_empty(ke_get(row, "title")),
                                    ke_or_empty(desc), i + 1));
    }
    o = json_object();
    json_set(o, "success", json_bool(true));
    {
        json_t *data = json_object();
        json_set(data, "web", web);
        json_set(o, "data", data);
    }
    return o;
}

json_t *w20_provider_9c3993_extract(const json_t *urls, const json_t *replies) {
    json_t *out = json_array();
    size_t n = (urls && urls->type == JSON_ARRAY) ? urls->c.count : 0;
    size_t i;

    for (i = 0; i < n; i++) {
        const json_t *url_v = urls->c.items[i];
        const char *url = (url_v && url_v->type == JSON_STRING && url_v->str_val)
                              ? url_v->str_val : "";
        const json_t *reply = (replies && replies->type == JSON_ARRAY && i < replies->c.count)
                                  ? replies->c.items[i] : NULL;
        const char *get_error = ke_text(reply, "get_error", "");
        int status = (int)json_get_num(reply, "status", 0);
        const char *text = ke_text(reply, "text", "");
        const char *json_error = ke_text(reply, "json_error", "");

        if (get_error[0]) {
            char *msg = ke_concat("Keenable extract failed: ", get_error);
            json_append(out, ke_page_error(url, msg));
            free(msg);
            continue;
        }
        if (status >= 400) {
            char *origin = ke_origin_error(status, text);
            if (origin) {
                json_append(out, ke_page_error(url, origin));
                free(origin);
                continue;
            }
            {
                char *detail = ke_status_detail(status, text);
                char *msg = ke_concat("Keenable extract failed: ", detail ? detail : "");
                json_append(out, ke_page_error(url, msg));
                free(detail);
                free(msg);
                continue;
            }
        }
        if (json_error[0]) {
            char *msg = ke_concat("Keenable extract failed: ", json_error);
            json_append(out, ke_page_error(url, msg));
            free(msg);
            continue;
        }
        {
            const json_t *body = ke_get(reply, "body");
            json_t *doc_url = ke_truthy(ke_get(body, "url"))
                                  ? json_copy(ke_get(body, "url")) : json_string(url);
            json_append(out, ke_document(doc_url,
                                         ke_or_empty(ke_get(body, "title")),
                                         ke_or_empty(ke_get(body, "content")), url));
        }
    }
    return out;
}

json_t *w20_provider_9c3993_setup_schema(void) {
    json_t *paid = json_object();
    json_t *env_vars = json_array();
    json_t *variant = json_array();
    json_t *o = json_object();

    {
        json_t *entry = json_object();
        json_set(entry, "key", json_string("KEENABLE_API_KEY"));
        json_set(entry, "prompt", json_string("Keenable API key"));
        json_set(entry, "url", json_string("https://keenable.ai"));
        json_append(env_vars, entry);
    }
    json_set(paid, "name", json_string("Keenable \xC2\xB7 Paid (API key)"));
    json_set(paid, "badge", json_string("paid"));
    json_set(paid, "tag", json_string(KE_PAID_TAG));
    json_set(paid, "env_vars", env_vars);
    json_set(paid, "web_tier", json_string("paid"));

    json_set(o, "name", json_string("Keenable \xC2\xB7 Free (keyless)"));
    json_set(o, "badge", json_string("free \xC2\xB7 no key"));
    json_set(o, "tag", json_string(KE_FREE_TAG));
    json_set(o, "env_vars", json_array());
    json_set(o, "web_tier", json_string("free"));
    json_append(variant, paid);
    json_set(o, "variants", variant);
    return o;
}

json_t *w20_provider_9c3993_identity(void) {
    json_t *o = json_object();
    json_set(o, "name", json_string("keenable"));
    json_set(o, "display_name", json_string("Keenable"));
    json_set(o, "key_env", json_string("KEENABLE_API_KEY"));
    json_set(o, "extract", json_bool(true));
    json_set(o, "keyless", json_bool(true));
    return o;
}
