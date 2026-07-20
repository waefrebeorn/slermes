/*
 * credential_persistence.c — Credential-pool disk-boundary sanitization helpers.
 * Port of Python agent/credential_persistence.py.
 *
 * Provides the SHARED, reusable sanitization primitives:
 *   credential_normalize_key()      — port of _normalize_key
 *   is_secret_payload_key()         — port of _is_secret_payload_key
 *   fingerprint_value()             — port of _fingerprint_value (SHA-256)
 *   credential_secret_fingerprint() — port of _credential_secret_fingerprint
 *
 * These are consumed by credential_pool_persistence.c (and any other
 * subsystem that needs disk-safe credential policy) — they are NOT
 * re-implemented there. Self-contained: minimal includes only, no god header.
 */

#include "credential_persistence.h"
#include "hermes_json.h"
#include "hermes_crypto.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================== *
 *  Safe metadata keys — never secret values
 * =============================================================== */

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

/* =============================================================== *
 *  credential_normalize_key — Port of Python _normalize_key()
 *  Lowercases, replaces hyphens/dots with underscores, inserts
 *  underscores at camelCase boundaries, strips surrounding space.
 * =============================================================== */

void credential_normalize_key(const char *key, char *out, size_t out_sz) {
    if (!key || !out || out_sz == 0) return;
    size_t j = 0;
    int prev_lower = 0;
    for (size_t i = 0; key[i] && j < out_sz - 1; i++) {
        char c = key[i];
        if (c == ' ' || c == '\t') { prev_lower = 0; continue; }
        if (c == '-' || c == '.') c = '_';
        if (isupper((unsigned char)c) && prev_lower) {
            if (j < out_sz - 2) out[j++] = '_';
        }
        prev_lower = islower((unsigned char)c) || isdigit((unsigned char)c);
        out[j++] = tolower((unsigned char)c);
    }
    out[j] = '\0';
}

/* =============================================================== *
 *  is_secret_payload_key — Port of Python _is_secret_payload_key()
 *  Returns true if the key names a secret payload field.
 * =============================================================== */

bool is_secret_payload_key(const char *key) {
    if (!key || !key[0]) return false;
    char normalized[128];
    credential_normalize_key(key, normalized, sizeof(normalized));
    if (is_safe_metadata_key(normalized)) return false;
    if (is_secret_value_key(normalized)) return true;
    for (int i = 0; SECRET_VALUE_SUFFIXES[i]; i++) {
        size_t nlen = strlen(normalized);
        size_t slen = strlen(SECRET_VALUE_SUFFIXES[i]);
        if (nlen > slen &&
            strcmp(normalized + nlen - slen, SECRET_VALUE_SUFFIXES[i]) == 0)
            return true;
    }
    return false;
}

/* =============================================================== *
 *  fingerprint_value — Port of Python _fingerprint_value()
 *  SHA-256 of the value, rendered as "sha256:<first 16 hex>".
 *  Returns NULL for NULL/empty input. Caller must free.
 * =============================================================== */

char *fingerprint_value(const char *value) {
    if (!value || !value[0]) return NULL;
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)value, strlen(value), digest);
    /* First 16 hex chars == first 8 bytes. */
    char *result = malloc(8 + 1 + 16 + 1);
    if (!result) return NULL;
    static const char hex[] = "0123456789abcdef";
    char *p = result;
    *p++ = 's'; *p++ = 'h'; *p++ = 'a'; *p++ = '2';
    *p++ = '5'; *p++ = '6'; *p++ = ':';
    for (int i = 0; i < 8; i++) {
        *p++ = hex[(digest[i] >> 4) & 0xf];
        *p++ = hex[digest[i] & 0xf];
    }
    *p = '\0';
    return result;
}

/* =============================================================== *
 *  credential_secret_fingerprint — Port of Python
 *  _credential_secret_fingerprint(). Walk a JSON payload object
 *  for secret-named values, return the first fingerprint found, or
 *  pass through an existing "sha256:..." fingerprint. Caller frees.
 * =============================================================== */

char *credential_secret_fingerprint(const json_t *payload) {
    if (!payload || payload->type != JSON_OBJECT) return NULL;

    /* Try well-known key names first (preserves Python ordering). */
    const char *well_known[] = {"agent_key", "access_token", "refresh_token",
                                 "api_key", "token", "secret", NULL};
    for (int i = 0; well_known[i]; i++) {
        const char *val = json_get_str(payload, well_known[i], NULL);
        if (val && val[0]) {
            char *fp = fingerprint_value(val);
            if (fp) return fp;
        }
    }

    /* Walk all keys for secret-like names. */
    for (size_t i = 0; i < payload->c.count; i++) {
        const char *key = payload->c.keys[i];
        if (!key) continue;
        if (!is_secret_payload_key(key)) continue;
        const json_t *val = payload->c.items[i];
        if (!val || val->type != JSON_STRING) continue;
        char *fp = fingerprint_value(val->str_val);
        if (fp) return fp;
    }

    /* Pass through an existing fingerprint if present. */
    const char *existing = json_get_str(payload, "secret_fingerprint", NULL);
    if (existing && strncmp(existing, "sha256:", 7) == 0)
        return strdup(existing);

    return NULL;
}
