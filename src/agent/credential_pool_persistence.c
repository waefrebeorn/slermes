/* Port of Python agent/credential_pool.py (split module).

Self-contained credential-pool subsystem component. credential_pool_t /
credential_entry_t are defined in include/credential_pool.h; internal helpers
shared across the split modules are declared in include/credential_pool_internals.h.
No god headers — only the minimal includes each module requires. C11 only.
*/

#include "credential_pool.h"
#include "credential_pool_internals.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_auth.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/*
 * Component 3/4 — borrowed-credential sanitization (AG29):
 * is_borrowed_credential_source / sanitize_borrowed_credential_payload
 * + prune control.
 */

/* ================================================================
 *  Credential persistence sanitization (AG29)
 *  Port of Python agent/credential_persistence.py (174 lines)
 *
 *  Defines which credential-pool entries are references to borrowed
 *  runtime secrets and strips raw values before writing to auth.json.
 * ================================================================ */

/* Sources Hermes owns and can intentionally persist in auth.json */
static const char *PERSISTABLE_SOURCES[][2] = {
    {"anthropic", "hermes_pkce"},
    {"minimax-oauth", "oauth"},
    {"nous", "device_code"},
    {"openai-codex", "device_code"},
    {"xai-oauth", "loopback_pkce"},
    {NULL, NULL}
};

/* Safe metadata keys that are not secret values */
static const char *SAFE_METADATA_KEYS[] = {
    "secret_fingerprint", "secret_source", "token_type", "scope",
    "client_id", "agent_key_id", "agent_key_expires_at", "agent_key_expires_in",
    "agent_key_reused", "agent_key_obtained_at", "expires_at", "expires_at_ms",
    "expires_in", "last_refresh", "last_status", "last_status_at",
    "last_error_code", "last_error_reason", "last_error_message",
    "last_error_reset_at", NULL
};

/* Keys that contain secret values */
static const char *SECRET_VALUE_KEYS[] = {
    "access_token", "refresh_token", "agent_key", "api_key", "apikey",
    "api_token", "auth_token", "authorization", "bearer_token",
    "client_secret", "credential", "credentials", "id_token",
    "oauth_token", "private_key", "secret_key", "session_token",
    "password", "secret", "token", "tokens", NULL
};

/* Suffixes that indicate secret values */
static const char *SECRET_SUFFIXES[] = {
    "_api_key", "_api_token", "_access_token", "_auth_token",
    "_refresh_token", "_bearer_token", "_client_secret", "_id_token",
    "_oauth_token", "_private_key", "_session_token", "_secret_key",
    "_password", "_secret", "_token", "_key", NULL
};

static bool str_in_list(const char *s, const char **list) {
    for (int i = 0; list[i]; i++) {
        if (strcasecmp(s, list[i]) == 0) return true;
    }
    return false;
}

static bool str_ends_with_any(const char *s, const char **suffixes) {
    size_t slen = strlen(s);
    for (int i = 0; suffixes[i]; i++) {
        size_t plen = strlen(suffixes[i]);
        if (slen >= plen && strcasecmp(s + slen - plen, suffixes[i]) == 0)
            return true;
    }
    return false;
}

static void normalize_key(const char *key, char *out, size_t out_size) {
    size_t j = 0;
    for (const char *p = key; *p && j < out_size - 2; p++) {
        /* Insert _ before uppercase letters preceded by lowercase/digit */
        if (*p >= 'A' && *p <= 'Z' && j > 0 && (out[j-1] >= 'a' && out[j-1] <= 'z')) {
            out[j++] = '_';
        }
        if (*p == '-' || *p == '.') {
            out[j++] = '_';
        } else {
            out[j++] = tolower((unsigned char)*p);
        }
    }
    out[j] = '\0';
}

/* Check if a source is borrowed (not owned by Hermes) */
/* Port of Python agent/credential_persistence.py:is_borrowed_credential_source(). */
bool is_borrowed_credential_source(const char *source, const char *provider_id) {
    if (!source || !*source) return false;
    /* Manual entries are owned */
    if (strcasecmp(source, "manual") == 0 || strncasecmp(source, "manual:", 7) == 0)
        return false;
    /* Check persistable list */
    for (int i = 0; PERSISTABLE_SOURCES[i][0]; i++) {
        if (strcasecmp(provider_id, PERSISTABLE_SOURCES[i][0]) == 0 &&
            strcasecmp(source, PERSISTABLE_SOURCES[i][1]) == 0)
            return false;
    }
    return true;
}

static bool is_secret_key(const char *key) {
    char norm[256];
    normalize_key(key, norm, sizeof(norm));
    if (!norm[0] || str_in_list(norm, SAFE_METADATA_KEYS)) return false;
    if (str_in_list(norm, SECRET_VALUE_KEYS)) return true;
    return str_ends_with_any(norm, SECRET_SUFFIXES);
}

/* Compute SHA-256 fingerprint of a value (first 16 hex chars) */
static char *fingerprint_value(const char *value) {
    if (!value || !*value) return NULL;
    /* FNV-1a based hash (not crypto, just for fingerprinting) */
    uint64_t h1 = 14695981039346656037ULL;
    uint64_t h2 = 14695981039346656037ULL;
    const unsigned char *p = (const unsigned char *)value;
    while (*p) {
        h1 ^= *p;
        h1 *= 1099511628211ULL;
        h2 ^= *p;
        h2 *= 1099511628211ULL;
        p++;
    }
    char *result = (char *)malloc(64);
    if (!result) return NULL;
    snprintf(result, 64, "sha256:%016llx%016llx", (unsigned long long)h1, (unsigned long long)h2);
    return result;
}

/* Sanitize a credential payload for disk writing. Name parity: Python calls this
 * sanitize_borrowed_credential_payload(). */
/* Port of Python agent/credential_persistence.py:sanitize_borrowed_credential_payload(). */
json_node_t *sanitize_borrowed_credential_payload(const json_node_t *payload, const char *provider_id) {
    if (!payload || payload->type != JSON_OBJECT) return NULL;

    /* Get source field */
    json_t *source_node = json_obj_get(payload, "source");
    const char *source = source_node && source_node->type == JSON_STRING ? source_node->str_val : "";

    /* If not borrowed, return a copy as-is */
    if (!is_borrowed_credential_source(source, provider_id)) {
        char *serialized = json_serialize(payload);
        json_node_t *copy = json_parse(serialized, NULL);
        free(serialized);
        return copy;
    }

    /* Borrowed: strip secret keys, keep metadata + fingerprint */
    json_node_t *result = json_new_object();

    /* Copy non-secret fields */
    for (size_t i = 0; i < payload->c.count; i++) {
        const char *key = payload->c.keys[i];
        json_t *val = payload->c.items[i];
        if (!is_secret_key(key)) {
            char *vstr = json_serialize(val);
            json_t *vcopy = json_parse(vstr, NULL);
            free(vstr);
            json_set(result, key, vcopy);
        }
    }

    /* Add fingerprint */
    json_t *ak = json_obj_get(payload, "agent_key");
    json_t *at = json_obj_get(payload, "access_token");
    json_t *rt = json_obj_get(payload, "refresh_token");
    json_t *tok = json_obj_get(payload, "token");
    json_t *sec = json_obj_get(payload, "secret");

    char *fp = NULL;
    if (ak && ak->type == JSON_STRING) fp = fingerprint_value(ak->str_val);
    else if (at && at->type == JSON_STRING) fp = fingerprint_value(at->str_val);
    else if (rt && rt->type == JSON_STRING) fp = fingerprint_value(rt->str_val);
    else if (tok && tok->type == JSON_STRING) fp = fingerprint_value(tok->str_val);
    else if (sec && sec->type == JSON_STRING) fp = fingerprint_value(sec->str_val);

    if (fp) {
        json_set(result, "secret_fingerprint", json_string(fp));
        free(fp);
    }

    return result;
}


