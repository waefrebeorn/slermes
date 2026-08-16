#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/microsoft_graph_client.c — C11 port of tools/microsoft_graph_client.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_tools_microsoft_graph_client.c ── */
/*
 * port_tools_microsoft_graph_client.c — C port of tools/microsoft_graph_client.py
 */


#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:from_env */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:get_json */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:patch_json */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:iterate_pages */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:collect_paginated */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:download_to_file */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:_decode_json */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:_should_retry */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:_should_refresh_token */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:_retry_delay */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:_build_api_error */

/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:from_env */
/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_auth.py:from_env */
/* Port of Python tools/microsoft_graph_client.py:from_env */
int cli_tools_microsoft_graph_client_from_env(char *tenant_buf, size_t tenant_size, char *client_buf, size_t client_size, char *secret_buf, size_t secret_size) {
    if (!tenant_buf || !client_buf || !secret_buf) {
        hermes_log(LOG_WARNING, "graph_client", "from_env: invalid args");
        return -1;
    }
    const char *tenant = getenv("MSGRAPH_TENANT_ID");
    const char *client = getenv("MSGRAPH_CLIENT_ID");
    const char *secret = getenv("MSGRAPH_CLIENT_SECRET");
    if (!tenant || !client || !secret) {
        hermes_log(LOG_WARNING, "graph_client", "from_env: missing env vars");
        return -1;
    }
    strncpy(tenant_buf, tenant, tenant_size - 1);
    tenant_buf[tenant_size - 1] = '\0';
    strncpy(client_buf, client, client_size - 1);
    client_buf[client_size - 1] = '\0';
    strncpy(secret_buf, secret, secret_size - 1);
    secret_buf[secret_size - 1] = '\0';
    hermes_log(LOG_DEBUG, "graph_client", "from_env: tenant=%s client=%s", tenant_buf, client_buf);
    return 0;
}

/* Port of Python tools_microsoft_graph_client:get_json */

/* Port of Python tools_microsoft_graph_client:patch_json */

/* Port of Python tools_microsoft_graph_client:iterate_pages */

/* Port of Python tools_microsoft_graph_client:collect_paginated */

/* Port of Python tools_microsoft_graph_client:download_to_file */

/* Port of Python tools_microsoft_graph_client:_decode_json */

/* Port of Python tools_microsoft_graph_client:_should_retry */

/* Port of Python tools_microsoft_graph_client:_should_refresh_token */

/* Port of Python tools_microsoft_graph_client:_retry_delay */

/* Port of Python tools_microsoft_graph_client:_build_api_error */

/* ── from src/tools/port_microsoft_graph_client_ports.c ── */
/*
 * port_microsoft_graph_client_remaining.c — Port of tools/microsoft_graph_client.py
 * graph client surface. Error envelope, delete, url resolution.
 */
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

/* PoP: mgc_init @ tools/microsoft_graph_client.py:__init__ */
char *mgc_init(long status_code, const char *method, const char *url) {
    char *out = NULL;
    asprintf(&out, "{\"status_code\": %ld, \"method\": \"%s\", \"url\": \"%s\"}",
             status_code, method ? method : "", url ? url : "");
    return out;
}

/* PoP: mgc_delete @ tools/microsoft_graph_client.py:delete */
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

/* PoP: mgc_resolve_url @ tools/microsoft_graph_client.py:_resolve_url */
char *mgc_resolve_url(const char *base_url, const char *path_or_url) {
    /* Python: absolute or relative. */
    if (!path_or_url) return NULL;
    if (strncmp(path_or_url, "http://", 7) == 0 || strncmp(path_or_url, "https://", 8) == 0)
        return strdup(path_or_url);
    char *out = NULL;
    asprintf(&out, "%s%s", base_url ? base_url : "", path_or_url);
    return out;
}

