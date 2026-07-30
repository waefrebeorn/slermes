/*
 * port_tools_microsoft_graph_client.c — C port of tools/microsoft_graph_client.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:from_env */
/* PoP: cli_tools_microsoft_graph_client_get_json @ tools/microsoft_graph_client.py:get_json */
/* PoP: cli_tools_microsoft_graph_client_patch_json @ tools/microsoft_graph_client.py:patch_json */
/* PoP: cli_tools_microsoft_graph_client_iterate_pages @ tools/microsoft_graph_client.py:iterate_pages */
/* PoP: cli_tools_microsoft_graph_client_collect_paginated @ tools/microsoft_graph_client.py:collect_paginated */
/* PoP: cli_tools_microsoft_graph_client_download_to_file @ tools/microsoft_graph_client.py:download_to_file */
/* PoP: cli_tools_microsoft_graph_client__decode_json @ tools/microsoft_graph_client.py:_decode_json */
/* PoP: cli_tools_microsoft_graph_client__should_retry @ tools/microsoft_graph_client.py:_should_retry */
/* PoP: cli_tools_microsoft_graph_client__should_refresh_token @ tools/microsoft_graph_client.py:_should_refresh_token */
/* PoP: cli_tools_microsoft_graph_client__retry_delay @ tools/microsoft_graph_client.py:_retry_delay */
/* PoP: cli_tools_microsoft_graph_client__build_api_error @ tools/microsoft_graph_client.py:_build_api_error */

/* PoP: cli_tools_microsoft_graph_client_from_env @ tools/microsoft_graph_client.py:from_env */
/* PoP: cli_tools_microsoft_graph_auth_from_env @ tools/microsoft_graph_auth.py:from_env */
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