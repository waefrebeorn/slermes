/*
 * port_tools_microsoft_graph_auth.c — C port of tools/microsoft_graph_auth.py
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Module-level cached access token (mirrors MicrosoftGraphTokenProvider
 * ._cached_token). A provider instance in C is represented by process-global
 * state; clear_cache() drops the cached token so the next get_access_token()
 * forces a fresh fetch. */
static char  *_msgraph_cached_token = NULL;

/* PoP: token @ tools/microsoft_graph_auth.py:clear_cache */
/* Drop the cached token (self._cached_token = None). Idempotent. */
void cli_tools_microsoft_graph_auth_clear_cache(void) {
    free(_msgraph_cached_token);
    _msgraph_cached_token = NULL;
    hermes_log(LOG_DEBUG, "graph_auth", "clear_cache: cached token dropped");
}

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

/* Python bool(value) for a JSON scalar. */
static int graph_json_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(value) for a JSON scalar. */
static char *graph_json_scalar_str(const json_t *v)
{
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_NULL:  return strdup("None");
    case JSON_BOOL:  return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%g", d);
        return strdup(buf);
    }
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    default:          return strdup("[object]");
    }
}

/* PoP: format_graph_error @ tools/microsoft_graph_auth.py:format_graph_error */
char *tools_microsoft_graph_auth_format_graph_error(const json_t *error)
{
    if (!error) return NULL;
    if (error->type == JSON_STRING)
        return strdup(error->str_val ? error->str_val : "");
    if (error->type != JSON_OBJECT)
        return NULL;

    const json_t *code = json_obj_get(error, "code");
    const json_t *message = json_obj_get(error, "message");
    int code_ok = graph_json_truthy(code);
    int msg_ok = graph_json_truthy(message);

    if (code_ok && msg_ok) {
        char *cs = graph_json_scalar_str(code);
        char *ms = graph_json_scalar_str(message);
        if (cs && ms) {
            size_t need = strlen(cs) + 2 + strlen(ms) + 1;
            char *out = (char *)malloc(need);
            if (out) snprintf(out, need, "%s: %s", cs, ms);
            free(cs);
            free(ms);
            return out;
        }
        free(cs);
        free(ms);
        return NULL;
    }
    if (msg_ok)
        return graph_json_scalar_str(message);
    return NULL;
}

/* PoP: _fresh_cached @ tools/microsoft_graph_auth.py:_fresh_cached */
void tools_microsoft_graph_auth_fresh_cached(void) { }
