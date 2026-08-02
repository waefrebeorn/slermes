/*
 * port_microsoft_graph_client_remaining.c — Port of tools/microsoft_graph_client.py
 * graph client surface. Error envelope, delete, url resolution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_http.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ tools/microsoft_graph_client.py:__init__ */
char *mgc_init(long status_code, const char *method, const char *url) {
    char *out = NULL;
    asprintf(&out, "{\"status_code\": %ld, \"method\": \"%s\", \"url\": \"%s\"}",
             status_code, method ? method : "", url ? url : "");
    return out;
}

/* PoP: delete @ tools/microsoft_graph_client.py:delete */
char *mgc_delete(const char *base_url, const char *path, const char *token) {
    /* Python: REAL DELETE request. */
    if (!base_url || !path) return NULL;
    char *url = NULL;
    if (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0)
        url = strdup(path);
    else
        asprintf(&url, "%s%s", base_url, path);
    http_t *h = http_new(20);
    if (!h) { free(url); return NULL; }
    char *hdr = NULL;
    if (token && *token) asprintf(&hdr, "Authorization: Bearer %s", token);
    http_resp_t *r = http_request(h, HTTP_DELETE, url, hdr, NULL, 0);
    char *out = NULL;
    if (r && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    free(hdr);
    free(url);
    return out;
}

/* PoP: _resolve_url @ tools/microsoft_graph_client.py:_resolve_url */
char *mgc_resolve_url(const char *base_url, const char *path_or_url) {
    /* Python: absolute or relative. */
    if (!path_or_url) return NULL;
    if (strncmp(path_or_url, "http://", 7) == 0 || strncmp(path_or_url, "https://", 8) == 0)
        return strdup(path_or_url);
    char *out = NULL;
    asprintf(&out, "%s%s", base_url ? base_url : "", path_or_url);
    return out;
}
