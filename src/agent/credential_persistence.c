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

/* PoP: credential_normalize_key @ agent/credential_persistence.py:_normalize_key */
void credential_normalize_key(const char *key, char *out, size_t out_sz) {
    if (!key || !out || out_sz == 0) return;
    /* Python: str(key).strip(), then lower/hyphen/dot only. Internal spaces
     * are preserved; only leading/trailing whitespace is removed. */
    for (;; key++) { char w = *key;
        if (w==' '||w=='\t'||w=='\n'||w=='\r'||w=='\f'||w=='\v') continue; break; }
    size_t len = strlen(key);
    while (len > 0) { char w = key[len-1];
        if (w==' '||w=='\t'||w=='\n'||w=='\r'||w=='\f'||w=='\v') { len--; continue; } break; }
    size_t j = 0;
    int prev_lower = 0;
    for (size_t i = 0; i < len && j < out_sz - 1; i++) {
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

/* =============================================================== *
 *  is_secret_payload_key — Port of Python _is_secret_payload_key()
 *  Returns true if the key names a secret payload field.
 * =============================================================== */

/* PoP: is_secret_payload_key @ agent/credential_persistence.py:_is_secret_payload_key */
bool is_secret_payload_key(const char *key) {
    if (!key || !key[0]) return false;
    char normalized[128];
    credential_normalize_key(key, normalized, sizeof(normalized));
    if (is_safe_metadata_key(normalized)) return false;
    if (is_secret_value_key(normalized)) return true;
    for (int i = 0; SECRET_VALUE_SUFFIXES[i]; i++) {
        size_t nlen = strlen(normalized);
        size_t slen = strlen(SECRET_VALUE_SUFFIXES[i]);
        if (nlen >= slen &&
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

/* PoP: fingerprint_secret_value @ agent/credential_persistence.py:fingerprint_secret_value */
char *fingerprint_secret_value(const void *value) {
    if (!value) return NULL;
    const char *s = (const char *)value;
    return fingerprint_value(s);
}

/* PoP: fingerprint_value @ agent/credential_persistence.py:_fingerprint_value */
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

/* PoP: credential_secret_fingerprint @ agent/credential_persistence.py:_credential_secret_fingerprint */
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

/* PoP: agent_credential_persistence_u_fingerprint_secret_value
 * @ agent/credential_persistence.py:fingerprint_secret_value */
/* None when the text is empty, else "sha256:<hexdigest()[:16]>". */
#include "crypto.h"

static void _hx2(const unsigned char *d, char *out, int n) {
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < n; i++) { out[2*i] = H[(d[i] >> 4) & 15]; out[2*i+1] = H[d[i] & 15]; }
    out[2*n] = 0;
}
char *agent_credential_persistence_u_fingerprint_secret_value(const char *arg) {
    const char *t = arg ? arg : "";
    if (!t[0]) return strdup("");
    unsigned char d[32]; crypto_sha256((const unsigned char *)t, strlen(t), d);
    char hex[33]; _hx2(d, hex, 32);
    char *out = malloc(24);
    /* "sha256:" + first 16 hex chars */
    snprintf(out, 24, "sha256:%.16s", hex);
    return out;
}

/* =============================================================== *
 *  is_borrowed_credential_source / sanitize_borrowed_credential_payload
 *  Ports of agent/credential_persistence.py. A "borrowed" row is a
 *  reference to someone else's secret: it may be listed on disk, but
 *  the raw secret must never be written there.
 * =============================================================== */

/* _PERSISTABLE_PROVIDER_SOURCES — (provider, source) pairs that own their secret. */
static const char *const CRED_PERSISTABLE[][2] = {
    { "xai-oauth",     "device_code" },
    { "anthropic",     "hermes_pkce" },
    { "openai-codex",  "device_code" },
    { "nous",          "device_code" },
    { "minimax-oauth", "oauth" },
};
#define CRED_PERSISTABLE_N ((int)(sizeof CRED_PERSISTABLE / sizeof CRED_PERSISTABLE[0]))

static void cred_strip_lower(const char *in, char *out, size_t cap) {
    if (!in) in = "";
    for (;; in++) {
        char w = *in;
        if (w==' '||w=='\t'||w=='\n'||w=='\r'||w=='\f'||w=='\v') continue;
        break;
    }
    size_t n = strlen(in);
    while (n > 0) {
        char w = in[n-1];
        if (w==' '||w=='\t'||w=='\n'||w=='\r'||w=='\f'||w=='\v') { n--; continue; }
        break;
    }
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; i++)
        out[o++] = (char)tolower((unsigned char)in[i]);
    out[o] = 0;
}

/* PoP: is_borrowed_credential_source @ agent/credential_persistence.py:is_borrowed_credential_source */
bool is_borrowed_credential_source(const char *source, const char *provider_id) {
    char src[256], prov[256];
    cred_strip_lower(source, src, sizeof src);
    if (!src[0] || strcmp(src, "manual") == 0 || strncmp(src, "manual:", 7) == 0)
        return false;
    cred_strip_lower(provider_id, prov, sizeof prov);
    for (int i = 0; i < CRED_PERSISTABLE_N; i++)
        if (strcmp(prov, CRED_PERSISTABLE[i][0]) == 0 &&
            strcmp(src,  CRED_PERSISTABLE[i][1]) == 0)
            return false;
    return true;
}

static int cred_cmp_key(const void *a, const void *b) {
    const char *const *x = a, *const *y = b;
    return strcmp(*x, *y);
}

/* PoP: sanitize_borrowed_credential_payload @ agent/credential_persistence.py:sanitize_borrowed_credential_payload
 * Owned sources pass through unchanged; borrowed ones keep metadata + a
 * fingerprint and lose every raw secret field. Returns a JSON object with
 * keys sorted (so the cup is order-stable); caller frees. */
char *sanitize_borrowed_credential_payload(const json_t *payload, const char *provider_id) {
    if (!payload || payload->type != JSON_OBJECT) return strdup("{}");
    const json_t *src = json_obj_get(payload, "source");
    const char *source = (src && src->type == JSON_STRING && src->str_val) ? src->str_val : "";
    int borrowed = is_borrowed_credential_source(source, provider_id);

    char *fingerprint = borrowed ? credential_secret_fingerprint(payload) : NULL;

    size_t cap = payload->c.count + 1;
    const char **keys = calloc(cap, sizeof *keys);
    const json_t **vals = calloc(cap, sizeof *vals);
    size_t n = 0;
    for (size_t i = 0; i < payload->c.count; i++) {
        const char *k = payload->c.keys[i];
        if (!k) continue;
        if (borrowed && is_secret_payload_key(k)) continue;
        if (borrowed && fingerprint && strcmp(k, "secret_fingerprint") == 0) continue;
        keys[n] = k; vals[n] = payload->c.items[i]; n++;
    }
    if (borrowed && fingerprint && *fingerprint) { keys[n] = "secret_fingerprint"; n++; }

    /* sort keys (and ride values along) */
    for (size_t i = 0; i + 1 < n; i++)
        for (size_t j = 0; j + 1 < n - i; j++)
            if (strcmp(keys[j], keys[j+1]) > 0) {
                const char *tk = keys[j]; keys[j] = keys[j+1]; keys[j+1] = tk;
                const json_t *tv = vals[j]; vals[j] = vals[j+1]; vals[j+1] = tv;
            }

    /* Serialise by hand: the values belong to `payload`, so they must not be
     * adopted into a second container (json_free of both would double-free). */
    char *js = malloc(2 + payload->c.count * 64 + (fingerprint ? strlen(fingerprint) + 24 : 0));
    size_t cap_out = js ? (2 + payload->c.count * 64 + (fingerprint ? strlen(fingerprint) + 24 : 0)) : 0;
    size_t used = 0;
    if (!js) { free(keys); free(vals); if (fingerprint) free(fingerprint); return strdup("{}"); }
    js[used++] = '{';
    for (size_t i = 0; i < n; i++) {
        const json_t *v = NULL;
        char *fpv = NULL;
        if (borrowed && fingerprint && strcmp(keys[i], "secret_fingerprint") == 0)
            fpv = json_serialize(json_string(fingerprint));
        else
            fpv = json_serialize((json_t *)vals[i]);
        const char *repr = fpv ? fpv : "null";
        size_t kl = strlen(keys[i]), vl = strlen(repr);
        size_t need = used + kl + vl + 8;
        if (need > cap_out) {
            cap_out = need * 2;
            char *nb = realloc(js, cap_out);
            if (!nb) { free(fpv); break; }
            js = nb;
        }
        if (i) js[used++] = ',';
        js[used++] = '"'; memcpy(js + used, keys[i], kl); used += kl; js[used++] = '"';
        js[used++] = ':';
        memcpy(js + used, repr, vl); used += vl;
        if (fpv) free(fpv);
        (void)v;
    }
    js[used++] = '}';
    js[used] = 0;
    free(keys); free(vals);
    if (fingerprint) free(fingerprint);
    return js;
}
