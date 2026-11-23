/*
 * managed_gateway_auth.c — C port of upstream Hermes tools/managed_gateway_auth.py.
 *
 * PoP: w14_managedgat_6ff729_connector_gateway_origin @ tools/managed_gateway_auth.py:connector_gateway_origin
 * PoP: w14_managedgat_6ff729_is_managed @ tools/managed_gateway_auth.py:is_managed_nous_gateway_url
 * PoP: w14_managedgat_6ff729_auth_headers @ tools/managed_gateway_auth.py:managed_gateway_auth_headers
 *
 * The Python module's injected seams (gateway_builder, token_reader and the
 * tools.managed_gateway_identity.user_agent value) are supplied by the fixture:
 * tool_url / connector_url stand in for build_vendor_gateway_url("tool"/"connector"),
 * and token stands in for read_nous_access_token().
 */
#include "noc_w14_managedgat_6ff729.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str.strip() for the ASCII whitespace the URL gates care about. */
static const char *trim_copy(const char *s, char *buf, size_t n) {
    size_t len;
    while (*s && isspace((unsigned char)*s)) s++;
    len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1])) len--;
    if (len >= n) len = n - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

static int scheme_char(int c) {
    return isalpha((unsigned char)c) || isdigit((unsigned char)c) ||
           c == '+' || c == '.' || c == '-';
}

typedef struct {
    char scheme[64];
    char netloc[1024];
    int  invalid;   /* urlsplit raised ValueError (e.g. malformed IPv6) */
} url_parts;

/* Faithful subset of urllib.parse.urlsplit(...)[:2]: (scheme, netloc). */
static void split_url(const char *url, url_parts *out) {
    const char *p;
    size_t n;

    out->scheme[0] = '\0';
    out->netloc[0] = '\0';
    out->invalid = 0;
    if (!url) return;

    while (*url && isspace((unsigned char)*url)) url++;

    p = url;
    if (isalpha((unsigned char)*p)) {
        const char *end = p;
        while (scheme_char((unsigned char)*end)) end++;
        if (*end == ':') {
            n = (size_t)(end - p);
            if (n >= sizeof(out->scheme)) n = sizeof(out->scheme) - 1;
            memcpy(out->scheme, p, n);
            out->scheme[n] = '\0';
            for (size_t i = 0; i < n; i++)
                out->scheme[i] = (char)tolower((unsigned char)out->scheme[i]);
            url = end + 1;
        }
    }

    if (url[0] == '/' && url[1] == '/') {
        const char *start, *end;
        url += 2;
        start = end = url;
        while (*end && *end != '/' && *end != '?' && *end != '#') {
            if (*end == '[') {
                const char *close = strchr(end, ']');
                if (!close) { out->invalid = 1; return; }
                end = close + 1;
                continue;
            }
            end++;
        }
        n = (size_t)(end - start);
        if (n >= sizeof(out->netloc)) n = sizeof(out->netloc) - 1;
        memcpy(out->netloc, start, n);
        out->netloc[n] = '\0';
    }
}

static int parts_equal(const url_parts *a, const url_parts *b) {
    return a->scheme[0] && !strcmp(a->scheme, b->scheme) &&
           !strcmp(a->netloc, b->netloc);
}

const char *w14_managedgat_6ff729_connector_gateway_origin(const char *connector_url) {
    return connector_url ? connector_url : "";
}

int w14_managedgat_6ff729_is_managed(const char *url, const char *tool_url,
                                     const char *connector_url) {
    char url_buf[8192];
    url_parts actual, tool, connector;

    if (!url || !*trim_copy(url, url_buf, sizeof url_buf)) return 0;

    split_url(tool_url, &tool);
    split_url(connector_url, &connector);
    if (tool.invalid || connector.invalid) return 0;

    split_url(url_buf, &actual);
    if (actual.invalid || !actual.scheme[0]) return 0;

    return parts_equal(&actual, &tool) || parts_equal(&actual, &connector) ? 1 : 0;
}

struct json_t *w14_managedgat_6ff729_auth_headers(const char *url, const char *tool_url,
                                                  const char *connector_url, const char *token) {
    char token_buf[8192];
    char bearer[8300];
    struct json_t *headers;

    if (!w14_managedgat_6ff729_is_managed(url, tool_url, connector_url))
        return json_object();

    if (!token || !*trim_copy(token, token_buf, sizeof token_buf))
        return json_object();

    headers = json_object();
    if (!headers) return NULL;
    snprintf(bearer, sizeof bearer, "Bearer %s", token_buf);
    json_set(headers, "Authorization", json_string(bearer));
    json_set(headers, "User-Agent", json_string("hermes-test/1.0"));
    return headers;
}
