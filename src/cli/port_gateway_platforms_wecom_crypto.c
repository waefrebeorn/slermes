/*
 * port_gateway_platforms_wecom_crypto.c — C port of gateway/platforms/wecom_crypto.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "libcrypto/crypto.h"
#include "libbase64/base64.h"
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* PoP: cli_gateway_platforms_wecom_crypto_encode @ gateway/platforms/wecom_crypto.py:encode */

/* Derive the 32-byte AES-256 key from corp_id + secret (SHA-256 of the
 * composite identity string). Faithful to the WeCom encoding contract: the
 * same corp_id/secret always yield the same key. */
static void wecom_derive_key(const char *corp_id, const char *secret,
                             unsigned char key[32]) {
    char seed[1024];
    int n = snprintf(seed, sizeof(seed), "%s:%s", corp_id ? corp_id : "",
                     secret ? secret : "");
    crypto_sha256((const unsigned char *)seed, (size_t)(n > 0 ? n : 0), key);
}

/* Port of Python gateway/platforms/wecom_crypto.py:encode */
/* Encode a WeCom message: real AES-256-GCM encrypt (random 12-byte IV) of the
 * plaintext, then base64url-encode the IV||ciphertext||tag blob. */
char *cli_gateway_platforms_wecom_crypto_encode(const char *corp_id, const char *secret,
                                                 const char *plaintext, int plaintext_len)
{
    if (!plaintext || plaintext_len <= 0) {
        return strdup("");
    }
    if (!corp_id || !corp_id[0]) {
        hermes_log(LOG_ERROR, "port",
                   "wecom_crypto: corp_id required for encode");
        return strdup("");
    }
    if (!secret || !secret[0]) {
        hermes_log(LOG_ERROR, "port",
                   "wecom_crypto: secret required for encode");
        return strdup("");
    }

    unsigned char key[32];
    wecom_derive_key(corp_id, secret, key);

    size_t ct_len = 0;
    unsigned char *ct = crypto_aes_encrypt((const unsigned char *)plaintext,
                                           (size_t)plaintext_len, key, 32, &ct_len);
    if (!ct) {
        hermes_log(LOG_ERROR, "port", "wecom_crypto: AES encrypt failed");
        return strdup("");
    }
    /* ct_len is IV(12)+ciphertext+tag(16); base64url-encode it. */
    char *result = crypto_base64url_encode(ct, ct_len);
    free(ct);

    hermes_log(LOG_DEBUG, "port",
               "wecom_crypto: encoded %d bytes for corp '%s'", plaintext_len, corp_id);
    return result ? result : strdup("");
}

/* PoP: cli_gateway_platforms_wecom_crypto_decode @ gateway/platforms/wecom_crypto.py:decode */

/* Port of Python gateway/platforms/wecom_crypto.py:decode */
/* Decode a WeCom message: base64url-decode the blob, then real AES-256-GCM
 * decrypt with the corp_id/secret-derived key. Returns plaintext. */
char *cli_gateway_platforms_wecom_crypto_decode(const char *corp_id, const char *secret,
                                                 const char *ciphertext, int ciphertext_len)
{
    if (!ciphertext || ciphertext_len <= 0) {
        return strdup("");
    }
    if (!corp_id || !corp_id[0]) {
        hermes_log(LOG_ERROR, "port",
                   "wecom_crypto: corp_id required for decode");
        return strdup("");
    }
    if (!secret || !secret[0]) {
        hermes_log(LOG_ERROR, "port",
                   "wecom_crypto: secret required for decode");
        return strdup("");
    }

    size_t raw_len = 0;
    unsigned char *raw = crypto_base64url_decode(ciphertext, &raw_len);
    if (!raw || raw_len < 12 + 16 + 1) {
        hermes_log(LOG_ERROR, "port", "wecom_crypto: decode: blob too short");
        free(raw);
        return strdup("");
    }

    unsigned char key[32];
    wecom_derive_key(corp_id, secret, key);

    size_t pt_len = 0;
    unsigned char *pt = crypto_aes_decrypt(raw, raw_len, key, 32, &pt_len);
    free(raw);
    if (!pt || pt_len == 0) {
        hermes_log(LOG_ERROR, "port", "wecom_crypto: decode: AES auth failed");
        free(pt);
        return strdup("");
    }

    char *result = (char *)malloc(pt_len + 1);
    if (result) {
        memcpy(result, pt, pt_len);
        result[pt_len] = '\0';
    }
    free(pt);

    hermes_log(LOG_DEBUG, "port",
               "wecom_crypto: decoded %zu bytes for corp '%s'", pt_len, corp_id);
    return result ? result : strdup("");
}

/* PoP: cli_gateway_platforms_wecom_crypto__encrypt_bytes @ gateway/platforms/wecom_crypto.py:_encrypt_bytes */

/* Port of Python gateway/platforms/wecom_crypto.py:_encrypt_bytes */
/* Low-level AES-256-GCM encrypt with caller-supplied key + IV.
 * Returns raw ciphertext bytes (IV || ciphertext || tag, 12+ct+16) as hex string,
 * matching the helper used by the WeCom handshake. Real encryption via OpenSSL. */
char *cli_gateway_platforms_wecom_crypto__encrypt_bytes(const char *key, int key_len,
                                                         const char *iv, int iv_len,
                                                         const char *data, int data_len)
{
    if (!data || data_len <= 0) return strdup("");
    if (!key || key_len <= 0) {
        hermes_log(LOG_ERROR, "port", "wecom_crypto: key required for encrypt_bytes");
        return strdup("");
    }
    if (!iv || iv_len <= 0) {
        hermes_log(LOG_ERROR, "port", "wecom_crypto: iv required for encrypt_bytes");
        return strdup("");
    }

    unsigned char aes_key[32];
    if (key_len >= 32) memcpy(aes_key, key, 32);
    else crypto_sha256((const unsigned char *)key, (size_t)key_len, aes_key);

    /* AES-256-GCM with explicit IV. */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return strdup("");
    unsigned char *out = NULL;
    int ok = 0;
    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL) != 1) break;
        if (EVP_EncryptInit_ex(ctx, NULL, NULL, aes_key, (const unsigned char *)iv) != 1) break;

        int ct_len = data_len + 16;
        out = (unsigned char *)malloc((size_t)ct_len + 16);
        int outl = 0;
        if (EVP_EncryptUpdate(ctx, out, &outl, (const unsigned char *)data, data_len) != 1) break;
        int pt_len = outl;
        if (EVP_EncryptFinal_ex(ctx, out + pt_len, &outl) != 1) break;
        pt_len += outl;

        /* Append the 16-byte GCM tag. */
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, out + pt_len) != 1) break;
        pt_len += 16;

        /* Return as hex string. */
        size_t hex_len = (size_t)pt_len * 2 + 1;
        char *hex = (char *)malloc(hex_len);
        if (hex) {
            for (int i = 0; i < pt_len; i++)
                snprintf(hex + i * 2, 3, "%02x", out[i]);
            EVP_CIPHER_CTX_free(ctx);
            free(out);
            hermes_log(LOG_DEBUG, "port",
                       "wecom_crypto: encrypted %d bytes (key_len=%d, iv_len=%d)",
                       data_len, key_len, iv_len);
            return hex;
        }
        ok = 1;
    } while (0);
    EVP_CIPHER_CTX_free(ctx);
    free(out);
    return strdup("");
}

/* PoP: cli_gateway_platforms_wecom_crypto__random_nonce @ gateway/platforms/wecom_crypto.py:_random_nonce */

/* Port of Python gateway/platforms/wecom_crypto.py:_random_nonce */
/* Generate a cryptographically random nonce of the given length.
 * Returns hex string. Real randomness via libcrypto (OpenSSL RAND_bytes). */
char *cli_gateway_platforms_wecom_crypto__random_nonce(int length)
{
    if (length <= 0) length = 16;
    if (length > 256) length = 256;

    unsigned char buf[256];
    if (!crypto_random_bytes(buf, (size_t)length)) {
        hermes_log(LOG_ERROR, "port", "wecom_crypto: random nonce generation failed");
        return strdup("");
    }

    size_t result_len = (size_t)length * 2 + 1;
    char *result = (char *)malloc(result_len);
    if (result) {
        int pos = 0;
        for (int i = 0; i < length; i++) {
            pos += sprintf(result + pos, "%02x", buf[i]);
        }
        result[pos] = '\0';
    }

    hermes_log(LOG_DEBUG, "port",
               "wecom_crypto: generated %d-byte random nonce", length);
    return result ? result : strdup("");
}
