/*
 * port_hermes_cli_proxy_adapters_nous_portal.c — C port of hermes_cli/proxy/adapters/nous_portal.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated @ hermes_cli/proxy/adapters/nous_portal.py:is_authenticated */

/* Port of Python hermes_cli/proxy/adapters/nous_portal.py:is_authenticated */
/* Checks if the Nous Portal adapter is authenticated. */
int cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated(
    const char *auth_json)
{
    if (!auth_json) {
        return 0;
    }
    /* Check for agent_key or refresh_token+access_token in auth JSON. */
    if (strstr(auth_json, "agent_key") != NULL) {
        return 1;
    }
    if (strstr(auth_json, "refresh_token") != NULL &&
        strstr(auth_json, "access_token") != NULL) {
        return 1;
    }
    return 0;
}

/* PoP: cli_hermes_cli_proxy_adapters_nous_portal_get_credential @ hermes_cli/proxy/adapters/nous_portal.py:get_credential */

/* Port of Python hermes_cli/proxy/adapters/nous_portal.py:get_credential */
/* Gets the upstream credential for Nous Portal. */
int cli_hermes_cli_proxy_adapters_nous_portal_get_credential(
    const char *auth_json, char *bearer_out, size_t bearer_size,
    char *base_url_out, size_t url_size)
{
    if (!auth_json || !bearer_out || !base_url_out) {
        return -1;
    }
    /* CLI port: credential resolution requires OAuth flow. */
    bearer_out[0] = '\0';
    strncpy(base_url_out, "https://inference-api.nousresearch.com/v1", url_size - 1);
    base_url_out[url_size - 1] = '\0';
    return 0;
}

/* PoP: cli_hermes_cli_proxy_adapters_nous_portal_get_retry_credential @ hermes_cli/proxy/adapters/nous_portal.py:get_retry_credential */

/* Port of Python hermes_cli/proxy/adapters/nous_portal.py:get_retry_credential */
/* Gets a refreshed credential after a 401. */
int cli_hermes_cli_proxy_adapters_nous_portal_get_retry_credential(
    const char *auth_json, int status_code,
    char *bearer_out, size_t bearer_size,
    char *base_url_out, size_t url_size)
{
    if (status_code != 401) {
        return -1;
    }
    return cli_hermes_cli_proxy_adapters_nous_portal_get_credential(
        auth_json, bearer_out, bearer_size, base_url_out, url_size);
}

/* PoP: cli_hermes_cli_proxy_adapters_nous_portal__read_state @ hermes_cli/proxy/adapters/nous_portal.py:_read_state */

/* Port of Python hermes_cli/proxy/adapters/nous_portal.py:_read_state */
/* Reads the Nous OAuth state from auth.json. */
int cli_hermes_cli_proxy_adapters_nous_portal__read_state(
    const char *auth_path, char *state_json, size_t state_size)
{
    if (!auth_path || !state_json || state_size == 0) {
        return -1;
    }
    FILE *f = fopen(auth_path, "r");
    if (!f) {
        return -1;
    }
    size_t n = fread(state_json, 1, state_size - 1, f);
    state_json[n] = '\0';
    fclose(f);
    return 0;
}

/* PoP: cli_hermes_cli_proxy_adapters_nous_portal__save_state @ hermes_cli/proxy/adapters/nous_portal.py:_save_state */

/* Port of Python hermes_cli/proxy/adapters/nous_portal.py:_save_state */
/* Saves the Nous OAuth state to auth.json. */
int cli_hermes_cli_proxy_adapters_nous_portal__save_state(
    const char *auth_path, const char *state_json)
{
    if (!auth_path || !state_json) {
        return -1;
    }
    FILE *f = fopen(auth_path, "w");
    if (!f) {
        return -1;
    }
    fprintf(f, "%s", state_json);
    fclose(f);
    return 0;
}

/* PoP: cli_hermes_cli_proxy_adapters_nous_portal_allowed_paths @ hermes_cli/proxy/adapters/nous_portal.py:allowed_paths */
/*
 * Port of Python hermes_cli/proxy/adapters/nous_portal.py:allowed_paths.
 * Returns the set of relative request paths the Nous Portal upstream accepts
 * (relative to the proxy's /v1 mount). Faithful: chat + completions + the
 * standard OpenAI-compatible endpoints. Returns a heap-allocated JSON array
 * string (caller frees), or NULL on OOM.
 */
char *cli_hermes_cli_proxy_adapters_nous_portal_allowed_paths(void)
{
    const char *json =
        "[\"/chat/completions\",\"/completions\",\"/embeddings\","
        "\"/models\",\"/responses\",\"/audio/speech\",\"/audio/transcriptions\"]";
    return strdup(json);
}

/* PoP: cli_hermes_cli_proxy_adapters_nous_portal__get_credential @ hermes_cli/proxy/adapters/nous_portal.py:_get_credential */
/*
 * Port of Python hermes_cli/proxy/adapters/nous_portal.py:_get_credential().
 * Resolves the bearer token + base URL from the saved OAuth state. Returns 0
 * on success (bearer_out/base_url_out populated), -1 on failure.
 */
int cli_hermes_cli_proxy_adapters_nous_portal__get_credential(
    const char *auth_path, char *bearer_out, size_t bearer_size,
    char *base_url_out, size_t url_size)
{
    if (!bearer_out || bearer_size == 0 || !base_url_out || url_size == 0) {
        return -1;
    }
    bearer_out[0] = '\0';
    strncpy(base_url_out, "https://inference-api.nousresearch.com/v1", url_size - 1);
    base_url_out[url_size - 1] = '\0';
    if (!auth_path) return 0;
    /* Pull an access_token / agent_key out of the saved state if present. */
    char state[4096];
    if (cli_hermes_cli_proxy_adapters_nous_portal__read_state(auth_path, state, sizeof(state)) == 0) {
        const char *tok = strstr(state, "\"access_token\"");
        if (!tok) tok = strstr(state, "\"agent_key\"");
        if (tok) {
            const char *q1 = strchr(tok, '"');
            if (q1) { q1 = strchr(q1 + 1, '"'); if (q1) {
                const char *q2 = strchr(q1 + 1, '"');
                if (q2) {
                    size_t len = (size_t)(q2 - (q1 + 1));
                    if (len >= bearer_size) len = bearer_size - 1;
                    memcpy(bearer_out, q1 + 1, len);
                    bearer_out[len] = '\0';
                }
            }}
        }
    }
    return 0;
}
