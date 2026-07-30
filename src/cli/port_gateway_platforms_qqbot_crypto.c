/*
 * port_gateway_platforms_qqbot_crypto.c — C port of gateway/platforms/qqbot/crypto.py
 */

#include "hermes_logger.h"
#include "libbase64/base64.h"
#include "libcrypto/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_platforms_qqbot_crypto_generate_bind_key @ gateway/platforms/qqbot/crypto.py:generate_bind_key */

/* Port of Python gateway/platforms/qqbot/crypto.py:generate_bind_key */
/* Generate a 256-bit random AES key and return it as base64 (caller free()). */
char *cli_gateway_platforms_qqbot_crypto_generate_bind_key(void)
{
    /* Generate 32 random bytes (256-bit key) from the OS RNG. */
    unsigned char key[32];
    if (!crypto_random_bytes(key, 32)) {
        hermes_log(LOG_ERROR, "qqbot_crypto", "Failed to generate random key");
        return NULL;
    }

    /* Standard base64 (matches Python base64.b64encode). Caller owns result. */
    return base64_encode(key, 32);
}

/* PoP: cli_gateway_platforms_qqbot_crypto_decrypt_secret @ gateway/platforms/qqbot/crypto.py:decrypt_secret */

/* Port of Python gateway/platforms/qqbot/crypto.py:decrypt_secret */
/* Decrypt a base64-encoded AES-256-GCM ciphertext.
 * Layout (after base64-decode): IV (12 bytes) || ciphertext || AuthTag (16 bytes).
 * Real decryption via libcrypto (OpenSSL AES-256-GCM). */
char *cli_gateway_platforms_qqbot_crypto_decrypt_secret(
    const char *encrypted_base64, const char *key_base64)
{
    if (!encrypted_base64 || !key_base64) {
        hermes_log(LOG_ERROR, "qqbot_crypto", "decrypt_secret: NULL argument");
        return NULL;
    }

    /* base64-decode the 32-byte AES key (matches Python base64.b64decode). */
    size_t key_len = 0;
    unsigned char *key = base64_decode(key_base64, &key_len);
    if (!key || key_len != 32) {
        hermes_log(LOG_ERROR, "qqbot_crypto", "decrypt_secret: bad key (need 32 bytes)");
        free(key);
        return NULL;
    }

    /* base64-decode the ciphertext blob. */
    size_t raw_len = 0;
    unsigned char *raw = base64_decode(encrypted_base64, &raw_len);
    if (!raw || raw_len < 12 + 16 + 1) {
        hermes_log(LOG_ERROR, "qqbot_crypto", "decrypt_secret: ciphertext too short");
        free(key);
        free(raw);
        return NULL;
    }

    /* crypto_aes_decrypt expects IV(12) || ciphertext || tag(16) — exactly the
     * decoded layout — and uses the raw key directly when key_len == 32. */
    size_t pt_len = 0;
    unsigned char *plaintext = crypto_aes_decrypt(raw, raw_len, key, key_len, &pt_len);
    free(key);
    free(raw);
    if (!plaintext || pt_len == 0) {
        hermes_log(LOG_ERROR, "qqbot_crypto",
                   "decrypt_secret: AES-GCM auth failed (wrong key / tampered)");
        free(plaintext);
        return NULL;
    }

    /* Return as a NUL-terminated UTF-8 string (Python returns .decode()). */
    char *out = (char *)malloc(pt_len + 1);
    if (!out) {
        free(plaintext);
        return NULL;
    }
    memcpy(out, plaintext, pt_len);
    out[pt_len] = '\0';
    free(plaintext);

    hermes_log(LOG_DEBUG, "qqbot_crypto", "decrypt_secret: decrypted %zu bytes", pt_len);
    return out;
}
