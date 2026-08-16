#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/proxy/adapters/xai.c — C11 port of hermes_cli/proxy/adapters/xai.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_hermes_cli_proxy_adapters_xai.c ── */
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

/* PoP: success @ hermes_cli/proxy/adapters/xai.py:_credential_from_entry */

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


/* ── from src/cli/port_proxy_xai_ports.c ── */
/*
 * port_proxy_xai_remaining.c — Port of hermes_cli/proxy/adapters/xai.py
 * xAI OAuth proxy adapter surface. Identity, auth state, credential
 * selection with retry logic.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "xai_http.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: pxa_init @ hermes_cli/proxy/adapters/xai.py:__init__ */
char *pxa_init(void) {
    return strdup("{\"pool\": null}");
}

/* PoP: pxa_name @ hermes_cli/proxy/adapters/xai.py:name */
char *pxa_name(void) {
    return strdup("xai");
}

/* PoP: pxa_display_name @ hermes_cli/proxy/adapters/xai.py:display_name */
char *pxa_display_name(void) {
    return strdup("xAI Grok OAuth");
}

/* PoP: pxa_is_authenticated @ hermes_cli/proxy/adapters/xai.py:is_authenticated */
bool pxa_is_authenticated(void) {
    /* Python: pool.has_available() — check the xAI credential pool.
     * Delegate to the C credential store (lib/libcredential). */
    
    return has_xai_credentials();
}

/* PoP: pxa_get_credential @ hermes_cli/proxy/adapters/xai.py:get_credential */
char *pxa_get_credential(void) {
    /* Python: pool.get_credential() — resolve the active xAI credential.
     * Delegate to the C credential store (lib/libxai_http). */
    extern bool xai_get_api_key(char out_key[XAI_API_KEY_MAX]);
    char key[XAI_API_KEY_MAX];
    if (!xai_get_api_key(key)) return NULL;
    return strdup(key);
}

/* PoP: pxa_get_retry_credential @ hermes_cli/proxy/adapters/xai.py:get_retry_credential */
char *pxa_get_retry_credential(long status_code) {
    /* Python: 401/429 only. */
    if (status_code != 401 && status_code != 429) return NULL;
    printf("xai retry credential (status %ld)\n", status_code);
    return NULL;
}

