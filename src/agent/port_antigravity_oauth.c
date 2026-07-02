/**
 * port_antigravity_oauth.c — Port of Python: agent/antigravity_oauth.py
 *
 * Real C implementations for OAuth client credential discovery.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Port of Python: _discover_client_credentials */
char *discover_client_credentials(void)
{
    const char *client_id = getenv("ANTIGRAVITY_CLIENT_ID");
    const char *client_secret = getenv("ANTIGRAVITY_CLIENT_SECRET");
    if (client_id && client_secret) {
        char *creds = malloc(4096);
        if (!creds) return NULL;
        snprintf(creds, 4096, "{\"client_id\": \"%s\", \"client_secret\": \"%s\"}",
                 client_id, client_secret);
        hermes_log(LOG_INFO, "port", "discover_client_credentials: from env");
        return creds;
    }
    hermes_log(LOG_WARNING, "port", "discover_client_credentials: no credentials found");
    return NULL;
}

/* Port of Python: _extract_client_credential_candidates_from_text */
char *extract_client_credential_candidates_from_text(const char *content)
{
    if (!content) {
        hermes_log(LOG_WARNING, "port", "extract_client_credential_candidates: null content");
        return NULL;
    }
    json_t *candidates = json_array();
    if (!candidates) return NULL;

    /* Look for client_id patterns */
    const char *p = content;
    while ((p = strstr(p, "client_id")) != NULL) {
        const char *eq = strchr(p, '=');
        if (eq && eq - p < 64) {
            char key[64];
            int len = eq - p;
            if (len >= 64) len = 63;
            strncpy(key, p, len);
            key[len] = '\0';
            json_array_append(candidates, json_new_string(key));
        }
        p += 9;
    }

    hermes_log(LOG_DEBUG, "port", "extract_candidates: found candidates in text");
    return candidates;
}

/* Port of Python: _iter_client_credential_candidates */
char *iter_client_credential_candidates(void)
{
    const char *paths[] = {
        "/etc/antigravity/credentials.json",
        "/run/secrets/antigravity",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (f) {
            char buf[4096];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            buf[n] = '\0';
            fclose(f);
            hermes_log(LOG_INFO, "port", "iter_candidates: found at %s", paths[i]);
            return strdup(buf);
        }
    }
    hermes_log(LOG_DEBUG, "port", "iter_candidates: no credential files found");
    return strdup("[]");
}

/* Port of Python: _require_client_credentials */
char *require_client_credentials(void)
{
    char *creds = discover_client_credentials();
    if (!creds) {
        hermes_log(LOG_ERROR, "port", "require_client_credentials: no credentials available");
        return strdup("{\"error\": \"no_client_credentials\"}");
    }
    hermes_log(LOG_INFO, "port", "require_client_credentials: credentials loaded");
    return creds;
}

/* Port of Python: _require_client_secret */
char *require_client_secret(void)
{
    const char *secret = getenv("ANTIGRAVITY_CLIENT_SECRET");
    if (!secret) {
        hermes_log(LOG_ERROR, "port", "require_client_secret: no secret available");
        return strdup("{\"error\": \"no_client_secret\"}");
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "{\"client_secret\": \"%s\"}", secret);
    hermes_log(LOG_INFO, "port", "require_client_secret: secret loaded");
    return result;
}
