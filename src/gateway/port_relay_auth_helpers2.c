/*
 * port_relay_auth_remaining2.c — Port of gateway/relay/auth.py token
 * verification surface. Constant-time verify, token parse/verify,
 * delivery signature verify.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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

/* PoP: verify_signature @ gateway/relay/auth.py:verify_signature */
bool rlauth_verify_signature(const char *sig_hex, const char *payload, const char *secret) {
    /* Python: constant-time HMAC check under ANY secret — real. */
    if (!sig_hex || !payload || !secret) return false;
    char mac[129];
    relay_auth_hmac_hex(secret, payload, mac, sizeof(mac));
    return ct_eq(mac, sig_hex);
}

/* PoP: verify_token @ gateway/relay/auth.py:verify_token */
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

/* PoP: verify_delivery_signature @ gateway/relay/auth.py:verify_delivery_signature */
bool rlauth_verify_delivery_signature(const char *body_json, const char *sig_hex, const char *secret) {
    /* Python: connector→gateway delivery check. */
    if (!body_json || !sig_hex || !secret) return false;
    return rlauth_verify_signature(sig_hex, body_json, secret);
}
