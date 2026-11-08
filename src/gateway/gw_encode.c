/*
 * gw_encode.c — canonical hex helper for the gateway layer.
 *
 * Consolidates the byte-identical `hex_encode` copies that previously lived
 * in platform/email.c, pairing.c and platform/webhook.c (and a near-twin in
 * agent/provider_bedrock.c, left for the agent-side home). One definition,
 * both return shapes callers need: a malloc'd string (alloc) and a
 * caller-owned buffer (buf).
 */
#include "hermes_encode.h"
#include <stdlib.h>

static const char HEX_DIGITS[] = "0123456789abcdef";

char *hex_encode_alloc(const unsigned char *data, size_t len) {
    if (!data && len > 0) return NULL;
    char *out = (char *)malloc(len * 2 + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = HEX_DIGITS[data[i] >> 4];
        out[i * 2 + 1] = HEX_DIGITS[data[i] & 0x0f];
    }
    out[len * 2] = '\0';
    return out;
}

void hex_encode_buf(const unsigned char *bin, int n, char *out) {
    for (int i = 0; i < n; i++) {
        out[i * 2]     = HEX_DIGITS[bin[i] >> 4];
        out[i * 2 + 1] = HEX_DIGITS[bin[i] & 0x0f];
    }
    out[n * 2] = '\0';
}
