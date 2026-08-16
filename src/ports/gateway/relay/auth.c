#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/relay/auth.c — C11 port of gateway/relay/auth.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/gateway/port_gateway_relay_auth.c ── */
/*
 * port_gateway_relay_auth.c — Port of Python gateway/relay/auth.py
 */

#include <stdio.h>
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include "libcrypto/crypto.h"


/* Port of Python: _delivery_payload */
typedef struct {
    char payload[4096];
    char signature[512];
    bool valid;
} relay_delivery_t;

/* PoP: relay_auth_delivery_payload @ gateway/relay/auth.py:_delivery_payload */
relay_delivery_t relay_auth_delivery_payload(const char *message_json) {
    relay_delivery_t result = {0};
    if (!message_json) return result;
    
    strncpy(result.payload, message_json, sizeof(result.payload) - 1);
    result.valid = true;
    return result;
}


/* Port of Python: _hmac_hex */
/* Real HMAC-SHA256 of `data` under `key`, rendered as a lowercase hex string. */
/* PoP: relay_auth_hmac_hex @ gateway/relay/auth.py:_hmac_hex */
/* PoP: relay_auth_hmac_hex @ gateway/relay/auth.py:sign */
void relay_auth_hmac_hex(const char *key, const char *data, char *output, size_t out_sz) {
    if (!key || !data || !output || out_sz == 0) return;

    unsigned char mac[CRYPTO_SHA256_LEN];
    crypto_hmac_sha256((const unsigned char *)key, strlen(key),
                       (const unsigned char *)data, strlen(data), mac);

    static const char hex[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < CRYPTO_SHA256_LEN && i * 2 + 1 < out_sz; i++) {
        output[i * 2]     = hex[mac[i] >> 4];
        output[i * 2 + 1] = hex[mac[i] & 0x0f];
    }
    output[i * 2] = '\0';
}


/* Port of Python: make_token */
/* PoP: relay_auth_make_token @ gateway/relay/auth.py:make_token */
void relay_auth_make_token(const char *secret, const char *payload, char *token_out, size_t out_sz) {
    if (!secret || !payload || !token_out || out_sz == 0) return;
    
    /* Create signed token: base64(payload).hmac(payload) */
    char hmac[512];
    relay_auth_hmac_hex(secret, payload, hmac, sizeof(hmac));
    
    snprintf(token_out, out_sz, "%s.%s", payload, hmac);
}


/* Port of Python: make_upgrade_token */
/* PoP: relay_auth_make_upgrade_token @ gateway/relay/auth.py:make_upgrade_token */
void relay_auth_make_upgrade_token(const char *secret, const char *ws_url, char *token_out, size_t out_sz) {
    if (!secret || !ws_url || !token_out || out_sz == 0) return;
    
    /* Create WebSocket upgrade token */
    relay_auth_make_token(secret, ws_url, token_out, out_sz);
}


/* Port of Python: sign */
void relay_auth_sign(const char *secret, const char *data, char *signature_out, size_t out_sz) {
    relay_auth_hmac_hex(secret, data, signature_out, out_sz);
}


/* Port of Python: verify_delivery_signature */
bool relay_auth_verify_delivery_signature(const char *secret, const char *payload, const char *signature) {
    if (!secret || !payload || !signature) return false;
    
    char expected[512];
    relay_auth_hmac_hex(secret, payload, expected, sizeof(expected));
    
    return (strcmp(expected, signature) == 0);
}


/* Port of Python: verify_signature */
bool relay_auth_verify_signature(const char *secret, const char *data, const char *signature) {
    return relay_auth_verify_delivery_signature(secret, data, signature);
}


/* Port of Python: verify_token */
bool relay_auth_verify_token(const char *secret, const char *token) {
    if (!secret || !token) return false;
    
    /* Split token into payload.signature */
    const char *dot = strchr(token, '.');
    if (!dot) return false;
    
    size_t payload_len = dot - token;
    char *payload = malloc(payload_len + 1);
    if (!payload) return false;
    memcpy(payload, token, payload_len);
    payload[payload_len] = '\0';
    
    bool result = relay_auth_verify_delivery_signature(secret, payload, dot + 1);
    free(payload);
    return result;
}



/* ── from src/gateway/port_relay_auth_helpers2.c ── */
/*
 * port_relay_auth_remaining2.c — Port of gateway/relay/auth.py token
 * verification surface. Constant-time verify, token parse/verify,
 * delivery signature verify.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* Forward decls — real impls in port_gateway_relay_auth.c. */
void relay_auth_hmac_hex(const char *key, const char *data, char *output, size_t out_sz);

static bool ct_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    size_t n = strlen(a);
    if (strlen(b) != n) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < n; i++) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

/* PoP: rlauth_verify_signature @ gateway/relay/auth.py:verify_signature */
bool rlauth_verify_signature(const char *sig_hex, const char *payload, const char *secret) {
    /* Python: constant-time HMAC check under ANY secret — real. */
    if (!sig_hex || !payload || !secret) return false;
    char mac[129];
    relay_auth_hmac_hex(secret, payload, mac, sizeof(mac));
    return ct_eq(mac, sig_hex);
}

/* PoP: rlauth_verify_token @ gateway/relay/auth.py:verify_token */
char *rlauth_verify_token(const char *token, const char *secret) {
    /* Python: split + verify + payload. */
    if (!token || !secret) return NULL;
    const char *dot = strrchr(token, '.');
    if (!dot) return NULL;
    char *payload = strndup(token, (size_t)(dot - token));
    char *sig = strdup(dot + 1);
    char mac[129];
    relay_auth_hmac_hex(secret, payload, mac, sizeof(mac));
    char *out = NULL;
    if (ct_eq(mac, sig)) out = strdup(payload);
    free(payload);
    free(sig);
    return out;
}

/* PoP: rlauth_verify_delivery_signature @ gateway/relay/auth.py:verify_delivery_signature */
bool rlauth_verify_delivery_signature(const char *body_json, const char *sig_hex, const char *secret) {
    /* Python: connector→gateway delivery check. */
    if (!body_json || !sig_hex || !secret) return false;
    return rlauth_verify_signature(sig_hex, body_json, secret);
}

