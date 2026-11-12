/*
 * port_web_common.c — Port of plugins/web/_common.py.
 *
 * Shared plumbing for the bundled web-search provider plugins: the result
 * shapes (key order is part of the contract — it reaches the model as JSON),
 * the origin-HTTP-status classifiers (the target page's own answer, not the
 * vendor API's), and the `hermes tools` picker schemas.
 */

#include "port_web_common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEB_ORIGIN_PREFIX "HTTP "
#define WEB_ORIGIN_INFIX " from origin"
#define WEB_EXA_HTTP "CRAWL_HTTP_"
#define WEB_EXA_NOT_FOUND "CRAWL_NOT_FOUND"
/* the middle dot in the keyless picker labels (U+00B7) */
#define WEB_MIDDOT "\xc2\xb7"

static int is_word_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Python str() of a json scalar; used by the row normalizer. */
static char *py_str(const json_t *v)
{
    if (!v || json_is_null(v))
        return strdup("None");
    if (json_is_string(v))
        return strdup(json_string_value(v) ? json_string_value(v) : "");
    if (json_is_bool(v))
        return strdup(json_bool_value(v) ? "True" : "False");
    if (json_is_number(v)) {
        char buf[64];
        double d = json_number_value(v);
        if (d == (double)(long long)d)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%g", d);
        return strdup(buf);
    }
    return strdup(json_serialize((json_t *)v));
}

/* --- Result shapes (key order is part of the contract) --- */

/* PoP: search_ok @ plugins/web/_common.py:search_ok */
json_t *webc_search_ok(json_t *web_results)
{
    json_t *out = json_new_object();
    json_object_set(out, "success", json_new_bool(1));
    json_t *data = json_new_object();
    json_object_set(data, "web", web_results ? json_copy(web_results) : json_new_array());
    json_object_set(out, "data", data);
    return out;
}

/* PoP: search_fail @ plugins/web/_common.py:search_fail */
json_t *webc_search_fail(const char *error)
{
    json_t *out = json_new_object();
    json_object_set(out, "success", json_new_bool(0));
    json_object_set(out, "error", json_new_string(error ? error : ""));
    return out;
}

/* PoP: web_hit @ plugins/web/_common.py:web_hit */
json_t *webc_web_hit(const char *url, const char *title, const char *description,
                     long position)
{
    json_t *out = json_new_object();
    json_object_set(out, "url", json_new_string(url ? url : ""));
    json_object_set(out, "title", json_new_string(title ? title : ""));
    json_object_set(out, "description", json_new_string(description ? description : ""));
    json_object_set(out, "position", json_new_number((double)position));
    return out;
}

/* PoP: title_hit @ plugins/web/_common.py:title_hit */
json_t *webc_title_hit(const char *title, const char *url, const char *description,
                       long position)
{
    json_t *out = json_new_object();
    json_object_set(out, "title", json_new_string(title ? title : ""));
    json_object_set(out, "url", json_new_string(url ? url : ""));
    json_object_set(out, "description", json_new_string(description ? description : ""));
    json_object_set(out, "position", json_new_number((double)position));
    return out;
}

/* PoP: document @ plugins/web/_common.py:document */
json_t *webc_document(const char *url, const char *title, const char *content,
                      const char *source_url)
{
    const char *src = source_url ? source_url : (url ? url : "");
    json_t *out = json_new_object();
    json_object_set(out, "url", json_new_string(url ? url : ""));
    json_object_set(out, "title", json_new_string(title ? title : ""));
    json_object_set(out, "content", json_new_string(content ? content : ""));
    json_object_set(out, "raw_content", json_new_string(content ? content : ""));
    json_t *meta = json_new_object();
    json_object_set(meta, "sourceURL", json_new_string(src));
    json_object_set(meta, "title", json_new_string(title ? title : ""));
    json_object_set(out, "metadata", meta);
    return out;
}

/* PoP: page_error @ plugins/web/_common.py:page_error */
json_t *webc_page_error(const char *url, const char *error)
{
    json_t *out = json_new_object();
    json_object_set(out, "url", json_new_string(url ? url : ""));
    json_object_set(out, "title", json_new_string(""));
    json_object_set(out, "content", json_new_string(""));
    json_object_set(out, "error", json_new_string(error ? error : ""));
    return out;
}

/* PoP: extract_fail @ plugins/web/_common.py:extract_fail */
json_t *webc_extract_fail(json_t *urls, const char *error)
{
    json_t *out = json_new_array();
    if (json_is_array(urls)) {
        size_t n = json_len(urls);
        for (size_t i = 0; i < n; i++) {
            json_t *u = json_get(urls, i);
            const char *s = json_is_string(u) ? json_string_value(u) : NULL;
            json_array_append(out, webc_page_error(s, error));
        }
    }
    return out;
}

/* PoP: _extract_interrupted @ plugins/web/_common.py:_extract_interrupted */
json_t *webc_extract_interrupted(json_t *urls)
{
    json_t *out = json_new_array();
    if (json_is_array(urls)) {
        size_t n = json_len(urls);
        for (size_t i = 0; i < n; i++) {
            json_t *u = json_get(urls, i);
            const char *s = json_is_string(u) ? json_string_value(u) : NULL;
            json_t *e = json_new_object();
            json_object_set(e, "url", json_new_string(s ? s : ""));
            json_object_set(e, "error", json_new_string("Interrupted"));
            json_object_set(e, "title", json_new_string(""));
            json_array_append(out, e);
        }
    }
    return out;
}

/* --- Origin HTTP status (the target page's answer, not the vendor API's) --- */

/* PoP: is_origin_http_error @ plugins/web/_common.py:is_origin_http_error */
int webc_is_origin_http_error(const char *error)
{
    if (!error)
        return 0;
    if (strncmp(error, WEB_ORIGIN_PREFIX, strlen(WEB_ORIGIN_PREFIX)) != 0)
        return 0;
    const char *p = error + strlen(WEB_ORIGIN_PREFIX);
    if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1]) ||
        !isdigit((unsigned char)p[2]))
        return 0;
    p += 3;
    if (strncmp(p, WEB_ORIGIN_INFIX, strlen(WEB_ORIGIN_INFIX)) != 0)
        return 0;
    p += strlen(WEB_ORIGIN_INFIX);
    return *p == '\0' || *p == ':';
}

/* PoP: origin_http_error @ plugins/web/_common.py:origin_http_error */
json_t *webc_origin_http_error(json_t *status, const char *reason)
{
    if (json_is_bool(status))
        return json_new_null();
    long code;
    if (json_is_number(status)) {
        code = (long)json_number_value(status);
    } else if (json_is_string(status)) {
        const char *s = json_string_value(status);
        if (!s || !*s)
            return json_new_null();
        char *end = NULL;
        code = strtol(s, &end, 10);
        if (end == s || (end && *end))
            return json_new_null();
    } else {
        return json_new_null();
    }
    if (code < 400)
        return json_new_null();

    /* detail = str(reason or "").strip() */
    char *detail = strdup(reason ? reason : "");
    if (detail) {
        size_t n = strlen(detail);
        while (n > 0 && isspace((unsigned char)detail[n - 1]))
            detail[--n] = '\0';
        char *p = detail;
        while (*p && isspace((unsigned char)*p))
            p++;
        if (p != detail) {
            memmove(detail, p, strlen(p) + 1);
        }
    }
    size_t len = (detail ? strlen(detail) : 0) + 32;
    char *out = (char *)malloc(len);
    if (detail && *detail)
        snprintf(out, len, "HTTP %ld from origin: %s", code, detail);
    else
        snprintf(out, len, "HTTP %ld from origin", code);
    free(detail);
    json_t *j = json_new_string(out);
    free(out);
    return j;
}

/* PoP: exa_crawl_status @ plugins/web/_common.py:exa_crawl_status */
json_t *webc_exa_crawl_status(const char *text)
{
    if (!text)
        return json_new_null();
    size_t n = strlen(text);
    while (n > 0 && isspace((unsigned char)text[n - 1]))
        n--;
    size_t start = n;
    while (start > 0 && !isspace((unsigned char)text[start - 1]))
        start--;
    size_t toklen = n - start;
    const char *tok = text + start;
    int at_boundary = (start == 0) || !is_word_char(text[start - 1]);

    if (at_boundary && toklen == strlen(WEB_EXA_NOT_FOUND) &&
        strncmp(tok, WEB_EXA_NOT_FOUND, toklen) == 0)
        return json_new_number(404);
    if (at_boundary && toklen >= strlen(WEB_EXA_HTTP) + 3 &&
        strncmp(tok, WEB_EXA_HTTP, strlen(WEB_EXA_HTTP)) == 0 &&
        isdigit((unsigned char)tok[strlen(WEB_EXA_HTTP)]) &&
        isdigit((unsigned char)tok[strlen(WEB_EXA_HTTP) + 1]) &&
        isdigit((unsigned char)tok[strlen(WEB_EXA_HTTP) + 2])) {
        const char *d = tok + strlen(WEB_EXA_HTTP);
        return json_new_number((d[0] - '0') * 100 + (d[1] - '0') * 10 + (d[2] - '0'));
    }
    return json_new_null();
}

static int keenable_message_match(const char *m)
{
    if (!m)
        return 0;
    if (strncasecmp(m, "the ", 4) != 0)
        return 0;
    m += 4;
    if (strncasecmp(m, "target server", 13) == 0) {
        m += 13;
    } else if (strncasecmp(m, "page does not exist", 19) == 0) {
        m += 19;
    } else {
        return 0;
    }
    return *m == '\0' || !is_word_char(*m);
}

/* PoP: keenable_origin_error @ plugins/web/_common.py:keenable_origin_error */
json_t *webc_keenable_origin_error(json_t *status, const char *body)
{
    json_t *data = json_parse(body ? body : "", NULL);
    if (!json_is_object(data)) {
        json_free(data);
        return json_new_null();
    }
    json_t *msg = json_obj_get(data, "message");
    char *message = NULL;
    if (json_is_string(msg)) {
        message = strdup(json_string_value(msg));
        size_t n = strlen(message);
        while (n > 0 && isspace((unsigned char)message[n - 1]))
            message[--n] = '\0';
        char *p = message;
        while (*p && isspace((unsigned char)*p))
            p++;
        if (p != message)
            memmove(message, p, strlen(p) + 1);
    }
    json_free(data);
    if (!message || !keenable_message_match(message)) {
        free(message);
        return json_new_null();
    }
    json_t *out = webc_origin_http_error(status, message);
    free(message);
    return out;
}

/* PoP: with_origin_status @ plugins/web/_common.py:with_origin_status */
json_t *webc_with_origin_status(json_t *entry, json_t *status, const char *reason)
{
    json_t *err = webc_origin_http_error(status, reason);
    if (!json_is_null(err))
        json_object_set(entry, "error", err);
    else
        json_free(err);
    return entry;
}

/* PoP: vendor_error_entry @ plugins/web/_common.py:vendor_error_entry */
json_t *webc_vendor_error_entry(const char *url, json_t *status,
                                const char *reason, const char *body)
{
    json_t *origin = webc_origin_http_error(status, NULL);
    json_t *entry;
    if (!json_is_null(origin)) {
        entry = webc_page_error(url, json_string_value(origin));
        json_free(origin);
        json_object_set(entry, "content", json_new_string(body ? body : ""));
    } else {
        const char *msg = body ? body : (reason ? reason : "extraction failed");
        entry = webc_page_error(url, msg);
    }
    json_t *meta = json_new_object();
    json_object_set(meta, "sourceURL", json_new_string(url ? url : ""));
    json_object_set(entry, "metadata", meta);
    return entry;
}

static int json_obj_has(const json_t *obj, const char *key)
{
    if (!json_is_object(obj))
        return 0;
    size_t n = json_object_size(obj);
    for (size_t i = 0; i < n; i++)
        if (strcmp(json_object_get_key_at(obj, i), key) == 0)
            return 1;
    return 0;
}

/* PoP: titled_rows @ plugins/web/_common.py:titled_rows */
json_t *webc_titled_rows(json_t *raw_results, const char *description_key)
{
    json_t *out = json_new_array();
    if (!json_is_array(raw_results))
        return out;
    size_t n = json_len(raw_results);
    for (size_t i = 0; i < n; i++) {
        json_t *r = json_get(raw_results, i);
        char *ft = NULL, *fu = NULL, *fd = NULL;
        const char *title = "", *url = "", *desc = "";
        if (json_is_object(r)) {
            /* r.get(key, "") — absent yields "", a present null yields str(None) */
            if (json_obj_has(r, "title")) {
                ft = py_str(json_obj_get(r, "title"));
                title = ft;
            }
            if (json_obj_has(r, "url")) {
                fu = py_str(json_obj_get(r, "url"));
                url = fu;
            }
            if (json_obj_has(r, description_key)) {
                fd = py_str(json_obj_get(r, description_key));
                desc = fd;
            }
        }
        json_array_append(out, webc_title_hit(title, url, desc, (long)i + 1));
        free(ft);
        free(fu);
        free(fd);
    }
    return out;
}

/* PoP: setup_schema @ plugins/web/_common.py:setup_schema */
json_t *webc_setup_schema(const char *name, const char *badge, const char *tag,
                          const char *key_env, const char *prompt, const char *url,
                          json_t *extra)
{
    json_t *out = json_new_object();
    json_object_set(out, "name", json_new_string(name ? name : ""));
    json_object_set(out, "badge", json_new_string(badge ? badge : ""));
    json_object_set(out, "tag", json_new_string(tag ? tag : ""));
    json_t *env_vars = json_new_array();
    if (key_env && *key_env) {
        json_t *e = json_new_object();
        json_object_set(e, "key", json_new_string(key_env));
        json_object_set(e, "prompt", json_new_string(prompt ? prompt : ""));
        json_object_set(e, "url", json_new_string(url ? url : ""));
        json_array_append(env_vars, e);
    }
    json_object_set(out, "env_vars", env_vars);
    if (json_is_object(extra)) {
        size_t n = json_object_size(extra);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(extra, i);
            json_t *v = json_object_get_at(extra, i);
            if (k && v)
                json_object_set(out, k, json_copy(v));
        }
    }
    return out;
}

/* PoP: keyless_variant_schema @ plugins/web/_common.py:keyless_variant_schema */
json_t *webc_keyless_variant_schema(const char *display, const char *key_env,
                                    const char *key_url, const char *free_tag,
                                    const char *paid_tag)
{
    char paid_name[256];
    char paid_prompt[256];
    char free_name[256];
    snprintf(paid_name, sizeof(paid_name), "%s " WEB_MIDDOT " Paid (API key)",
             display ? display : "");
    snprintf(paid_prompt, sizeof(paid_prompt), "%s API key", display ? display : "");
    snprintf(free_name, sizeof(free_name), "%s " WEB_MIDDOT " Free (keyless)",
             display ? display : "");

    json_t *extra = json_new_object();
    json_object_set(extra, "web_tier", json_new_string("paid"));
    json_t *paid = webc_setup_schema(paid_name, "paid", paid_tag, key_env,
                                     paid_prompt, key_url, extra);

    json_t *free_extra = json_new_object();
    json_t *variants = json_new_array();
    json_array_append(variants, paid);
    json_object_set(free_extra, "web_tier", json_new_string("free"));
    json_object_set(free_extra, "variants", variants);
    return webc_setup_schema(free_name, "free \xc2\xb7 no key", free_tag,
                             NULL, NULL, NULL, free_extra);
}
