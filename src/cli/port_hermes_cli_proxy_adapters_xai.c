/*
 * port_hermes_cli_proxy_adapters_xai.c — C port of hermes_cli/proxy/adapters/xai.py
 *
 * xAI OAuth upstream adapter. Ports the concrete pieces not yet covered:
 * allowed_paths (relative request paths) and _credential_from_entry (resolve
 * a bearer token + base URL from a pooled credential entry).
 */

/* PoP: cli_hermes_cli_proxy_adapters_xai_allowed_paths @ hermes_cli/proxy/adapters/xai.py:allowed_paths */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Port of Python hermes_cli/proxy/adapters/xai.py:allowed_paths.
 * Returns the set of relative request paths the xAI upstream accepts. */
char *cli_hermes_cli_proxy_adapters_xai_allowed_paths(void)
{
    const char *json =
        "[\"/chat/completions\",\"/completions\",\"/embeddings\","
        "\"/models\",\"/responses\",\"/audio/speech\",\"/audio/transcriptions\"]";
    return strdup(json);
}

/* PoP: cli_hermes_cli_proxy_adapters_xai__credential_from_entry @ hermes_cli/proxy/adapters/xai.py:_credential_from_entry */

/* Port of Python hermes_cli/proxy/adapters/xai.py:_credential_from_entry().
 * Resolves a bearer token + base URL from a pooled credential entry. The entry
 * is passed as a JSON string with optional runtime_api_key / access_token and
 * runtime_base_url / base_url fields. Returns 0 on success (bearer_out /
 * base_url_out populated), -1 when no token is present. */
int cli_hermes_cli_proxy_adapters_xai__credential_from_entry(
    const char *entry_json, char *bearer_out, size_t bearer_size,
    char *base_url_out, size_t url_size)
{
    const char *def_base = "https://api.x.ai/v1";
    if (!bearer_out || bearer_size == 0 || !base_url_out || url_size == 0) {
        return -1;
    }
    bearer_out[0] = '\0';
    strncpy(base_url_out, def_base, url_size - 1);
    base_url_out[url_size - 1] = '\0';

    if (!entry_json) return -1;

    /* Prefer runtime_api_key, fall back to access_token. */
    const char *tok = strstr(entry_json, "\"runtime_api_key\"");
    if (!tok) tok = strstr(entry_json, "\"access_token\"");
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
    if (!bearer_out[0]) return -1;  /* no token -> raise equivalent */

    /* Optional base URL override. */
    const char *bu = strstr(entry_json, "\"runtime_base_url\"");
    if (!bu) bu = strstr(entry_json, "\"base_url\"");
    if (bu) {
        const char *q1 = strchr(bu, '"');
        if (q1) { q1 = strchr(q1 + 1, '"'); if (q1) {
            const char *q2 = strchr(q1 + 1, '"');
            if (q2) {
                size_t len = (size_t)(q2 - (q1 + 1));
                if (len >= url_size) len = url_size - 1;
                if (len > 0) {
                    memcpy(base_url_out, q1 + 1, len);
                    base_url_out[len] = '\0';
                }
            }
        }}
    }
    /* strip a trailing slash from base URL */
    size_t bl = strlen(base_url_out);
    while (bl > 0 && base_url_out[bl - 1] == '/') base_url_out[--bl] = '\0';
    return 0;
}
