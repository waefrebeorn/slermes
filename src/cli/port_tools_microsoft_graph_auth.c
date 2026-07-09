/*
 * port_tools_microsoft_graph_auth.c — C port of tools/microsoft_graph_auth.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PoP: cli_tools_microsoft_graph_auth_token_url @ tools/microsoft_graph_auth.py:token_url */
int cli_tools_microsoft_graph_auth_token_url(const char *tenant_id, const char *authority, char *buf, size_t bufsize) {
    if (!tenant_id || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "graph_auth", "token_url: invalid args");
        return -1;
    }
    if (!authority) authority = "https://login.microsoftonline.com";
    snprintf(buf, bufsize, "%s/%s/oauth2/v2.0/token", authority, tenant_id);
    hermes_log(LOG_DEBUG, "graph_auth", "token_url: %s", buf);
    return 0;
}

/* PoP: cli_tools_microsoft_graph_auth_from_env @ tools/microsoft_graph_auth.py:from_env */
int cli_tools_microsoft_graph_auth_from_env(char *tenant_buf, size_t tenant_size, char *client_buf, size_t client_size, char *secret_buf, size_t secret_size) {
    if (!tenant_buf || !client_buf || !secret_buf) return -1;
    const char *tenant = getenv("MSGRAPH_TENANT_ID");
    const char *client = getenv("MSGRAPH_CLIENT_ID");
    const char *secret = getenv("MSGRAPH_CLIENT_SECRET");
    if (!tenant || !client || !secret) {
        hermes_log(LOG_WARNING, "graph_auth", "from_env: missing env vars");
        return -1;
    }
    strncpy(tenant_buf, tenant, tenant_size - 1);
    tenant_buf[tenant_size - 1] = '\0';
    strncpy(client_buf, client, client_size - 1);
    client_buf[client_size - 1] = '\0';
    strncpy(secret_buf, secret, secret_size - 1);
    secret_buf[secret_size - 1] = '\0';
    hermes_log(LOG_DEBUG, "graph_auth", "from_env: tenant=%s client=%s", tenant_buf, client_buf);
    return 0;
}

/* PoP: cli_tools_microsoft_graph_auth_is_expired @ tools/microsoft_graph_auth.py:is_expired */
int cli_tools_microsoft_graph_auth_is_expired(time_t expires_at, int skew_seconds) {
    time_t now = time(NULL);
    int expired = (expires_at <= now + skew_seconds);
    hermes_log(LOG_DEBUG, "graph_auth", "is_expired: expires=%ld now=%ld skew=%d -> %d",
               (long)expires_at, (long)now, skew_seconds, expired);
    return expired;
}

/* PoP: cli_tools_microsoft_graph_auth_expires_in_seconds @ tools/microsoft_graph_auth.py:expires_in_seconds */
int cli_tools_microsoft_graph_auth_expires_in_seconds(time_t expires_at) {
    time_t now = time(NULL);
    int remaining = (int)difftime(expires_at, now);
    if (remaining < 0) remaining = 0;
    hermes_log(LOG_DEBUG, "graph_auth", "expires_in_seconds: %d", remaining);
    return remaining;
}



/* PoP: cli_tools_microsoft_graph_auth_inspect_token_health @ tools/microsoft_graph_auth.py:inspect_token_health */
int cli_tools_microsoft_graph_auth_inspect_token_health(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) return -1;
    const char *tenant = getenv("MSGRAPH_TENANT_ID");
    const char *client = getenv("MSGRAPH_CLIENT_ID");
    snprintf(buf, bufsize, "{\"configured\":%s,\"tenant\":\"%s\",\"client\":\"%s\"}",
             tenant && client ? "true" : "false",
             tenant ? tenant : "",
             client ? client : "");
    hermes_log(LOG_DEBUG, "graph_auth", "inspect_token_health: %s", buf);
    return 0;
}

/* PoP: cli_tools_microsoft_graph_auth_get_access_token @ tools/microsoft_graph_auth.py:get_access_token */
int cli_tools_microsoft_graph_auth_get_access_token(const char *tenant_id, const char *client_id, const char *client_secret, char *buf, size_t bufsize) {
    if (!tenant_id || !client_id || !client_secret || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "graph_auth", "get_access_token: invalid args");
        return -1;
    }
    hermes_log(LOG_DEBUG, "graph_auth", "get_access_token: tenant=%s client=%s (HTTP fetch not available in C)", tenant_id, client_id);
    buf[0] = '\0';
    return -1;
}

/* PoP: cli_tools_microsoft_graph_auth__fetch_access_token @ tools/microsoft_graph_auth.py:_fetch_access_token */
int cli_tools_microsoft_graph_auth__fetch_access_token(const char *token_url, const char *client_id, const char *client_secret, char *buf, size_t bufsize) {
    if (!token_url || !client_id || !client_secret || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "graph_auth", "_fetch_access_token: invalid args");
        return -1;
    }
    hermes_log(LOG_DEBUG, "graph_auth", "_fetch_access_token: url=%s (HTTP fetch not available in C)", token_url);
    buf[0] = '\0';
    return -1;
}

/* PoP: cli_tools_microsoft_graph_auth__extract_error_detail @ tools/microsoft_graph_auth.py:_extract_error_detail */
int cli_tools_microsoft_graph_auth__extract_error_detail(const char *response_body, char *buf, size_t bufsize) {
    if (!response_body || !buf || bufsize == 0) return -1;
    /* Try to extract error_description from JSON */
    const char *key = "\"error_description\"";
    const char *p = strstr(response_body, key);
    if (p) {
        p += strlen(key);
        while (*p == ' ' || *p == ':') p++;
        if (*p == '"') {
            p++;
            size_t j = 0;
            while (*p && *p != '"' && j < bufsize - 1) {
                buf[j++] = *p++;
            }
            buf[j] = '\0';
            hermes_log(LOG_DEBUG, "graph_auth", "_extract_error_detail: %s", buf);
            return 0;
        }
    }
    /* Fallback: return first 200 chars of response */
    strncpy(buf, response_body, bufsize - 1);
    buf[bufsize - 1] = '\0';
    if (strlen(buf) > 200) buf[200] = '\0';
    return 0;
}
