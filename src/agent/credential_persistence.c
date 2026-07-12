/*
 * credential_persistence.c — Credential-pool disk-boundary sanitization helpers.
 * Port of Python agent/credential_persistence.py (174 lines).
 *
 * Provides _normalize_key(), _is_secret_payload_key(), and _fingerprint_value()
 * used by credential_pool.c's sanitize_borrowed_credential_payload().
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Safe metadata keys — never secret values
 * ================================================================ */

static bool is_safe_metadata_key(const char *key) {
    static const char *safe_keys[] = {
        "secret_fingerprint", "secret_source", "token_type", "scope",
        "client_id", "agent_key_id", "agent_key_expires_at",
        "agent_key_expires_in", "agent_key_reused", "agent_key_obtained_at",
        "expires_at", "expires_at_ms", "expires_in", "last_refresh",
        "last_status", "last_status_at", "last_error_code",
        "last_error_reason", "last_error_message", "last_error_reset_at",
        NULL
    };
    for (int i = 0; safe_keys[i]; i++)
        if (strcmp(key, safe_keys[i]) == 0) return true;
    return false;
}

static bool is_secret_value_key(const char *key) {
    static const char *secret_keys[] = {
        "access_token", "refresh_token", "agent_key", "api_key",
        "apikey", "api_token", "auth_token", "authorization",
        "bearer_token", "client_secret", "credential", "credentials",
        "id_token", "oauth_token", "private_key", "secret_key",
        "session_token", "password", "secret", "token", "tokens",
        NULL
    };
    for (int i = 0; secret_keys[i]; i++)
        if (strcmp(key, secret_keys[i]) == 0) return true;
    return false;
}

static const char *SECRET_VALUE_SUFFIXES[] = {
    "_api_key", "_api_token", "_access_token", "_auth_token",
    "_refresh_token", "_bearer_token", "_client_secret", "_id_token",
    "_oauth_token", "_private_key", "_session_token", "_secret_key",
    "_password", "_secret", "_token", "_key",
    NULL
};

/* ================================================================
 *  _normalize_key — Port of Python _normalize_key()
 * ================================================================ */

/* Port of Python agent/credential_persistence.py:_normalize_key().
 * Lowercases, replaces hyphens/dots with underscores, inserts
 * underscores at camelCase boundaries. */
static void normalize_key(const char *key, char *out, size_t out_sz) {
    if (!key || !out || out_sz == 0) return;
    size_t j = 0;
    int prev_lower = 0;
    for (size_t i = 0; key[i] && j < out_sz - 1; i++) {
        char c = key[i];
        if (c == '-' || c == '.') c = '_';
        if (isupper((unsigned char)c) && prev_lower) {
            if (j < out_sz - 2) out[j++] = '_';
        }
        prev_lower = islower((unsigned char)c) || isdigit((unsigned char)c);
        out[j++] = tolower((unsigned char)c);
    }
    out[j] = '\0';
}

/* ================================================================
 *  _is_secret_payload_key — Port of Python _is_secret_payload_key()
 * ================================================================ */

/* Port of Python agent/credential_persistence.py:_is_secret_payload_key().
 * Returns true if the key looks like a secret payload field. */
bool is_secret_payload_key(const char *key) {
    if (!key || !key[0]) return false;
    char normalized[128];
    normalize_key(key, normalized, sizeof(normalized));
    if (is_safe_metadata_key(normalized)) return false;
    if (is_secret_value_key(normalized)) return true;
    for (int i = 0; SECRET_VALUE_SUFFIXES[i]; i++) {
        size_t nlen = strlen(normalized);
        size_t slen = strlen(SECRET_VALUE_SUFFIXES[i]);
        if (nlen > slen && strcmp(normalized + nlen - slen, SECRET_VALUE_SUFFIXES[i]) == 0)
            return true;
    }
    return false;
}

/* ================================================================
 *  _fingerprint_value — Port of Python _fingerprint_value()
 * ================================================================ */

/* Port of Python agent/credential_persistence.py:_fingerprint_value().
 * Returns a hash-based fingerprint (first 16 hex chars),
 * or NULL if the value is NULL or empty. Caller must free. */
char *fingerprint_value(const char *value) {
    if (!value || !value[0]) return NULL;
    unsigned long hash = 5381;
    for (const char *p = value; *p; p++)
        hash = ((hash << 5) + hash) + (unsigned char)*p;
    char *result = malloc(24);
    if (!result) return NULL;
    snprintf(result, 24, "hash:%016lx", hash);
    return result;
}

/* Port of Python agent/credential_persistence.py:_credential_secret_fingerprint().
 * Walk a JSON payload object looking for secret key fields and return
 * the first fingerprint found. Returns a malloc'd string or NULL. */
char *credential_secret_fingerprint(const json_t *payload) {
    if (!payload || payload->type != JSON_OBJECT) return NULL;

    /* Try well-known key names first */
    const char *well_known[] = {"agent_key", "access_token", "refresh_token",
                                 "api_key", "token", "secret", NULL};
    for (int i = 0; well_known[i]; i++) {
        const char *val = json_get_str(payload, well_known[i], NULL);
        if (val && val[0]) {
            char *fp = fingerprint_value(val);
            if (fp) return fp;
        }
    }

    /* Walk all keys for secret-like names */
    for (size_t i = 0; i < payload->c.count; i++) {
        const char *key = payload->c.keys[i];
        if (!key) continue;
        if (!is_secret_payload_key(key)) continue;
        const json_t *val = payload->c.items[i];
        if (!val || val->type != JSON_STRING) continue;
        char *fp = fingerprint_value(val->str_val);
        if (fp) return fp;
    }

    /* Return existing fingerprint if present */
    const char *existing = json_get_str(payload, "secret_fingerprint", NULL);
    if (existing && strncmp(existing, "sha256:", 7) == 0)
        return strdup(existing);

    return NULL;
}
