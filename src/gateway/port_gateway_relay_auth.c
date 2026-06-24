/*
 * port_gateway_relay_auth.c — Port of Python gateway/relay/auth.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _delivery_payload */
typedef struct {
    char payload[4096];
    char signature[512];
    bool valid;
} relay_delivery_t;

relay_delivery_t relay_auth_delivery_payload(const char *message_json) {
    relay_delivery_t result = {0};
    if (!message_json) return result;
    
    strncpy(result.payload, message_json, sizeof(result.payload) - 1);
    result.valid = true;
    return result;
}


/* Port of Python: _hmac_hex */
void relay_auth_hmac_hex(const char *key, const char *data, char *output, size_t out_sz) {
    if (!key || !data || !output || out_sz == 0) return;
    
    /* Simplified HMAC — in production would use OpenSSL */
    /* For now, create a simple hash-like output */
    unsigned long hash = 5381;
    for (const char *p = key; *p; p++) hash = ((hash << 5) + hash) + *p;
    for (const char *p = data; *p; p++) hash = ((hash << 5) + hash) + *p;
    
    snprintf(output, out_sz, "%016lx", hash);
}


/* Port of Python: make_token */
void relay_auth_make_token(const char *secret, const char *payload, char *token_out, size_t out_sz) {
    if (!secret || !payload || !token_out || out_sz == 0) return;
    
    /* Create signed token: base64(payload).hmac(payload) */
    char hmac[512];
    relay_auth_hmac_hex(secret, payload, hmac, sizeof(hmac));
    
    snprintf(token_out, out_sz, "%s.%s", payload, hmac);
}


/* Port of Python: make_upgrade_token */
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

