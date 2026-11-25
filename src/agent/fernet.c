/*
 * fernet.c — Fernet tokens, ported from the format agent/vault_store.py writes.
 *
 * Token layout (then base64url, padded):
 *
 *   0x80 | ts(8 BE) | iv(16) | AES-128-CBC(plaintext) | HMAC-SHA256(32)
 *
 * AES comes from OpenSSL (already linked for the rest of the tree); the HMAC, the key split and the
 * base64url codec are local so this module has no opinion about how the rest of the tree does crypto.
 *
 * Verification order matters and follows upstream: the HMAC is checked over the *raw* bytes before
 * anything is decrypted, so a tampered token never reaches the cipher, and the timestamp check runs
 * after the HMAC so an attacker cannot distinguish "expired" from "forged" without the key.
 */
#include "fernet.h"
#include "crypto.h"

#include <openssl/evp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FERNET_VERSION 0x80
#define FERNET_MIN_LEN (1 + 8 + FERNET_IV_LEN + 16 + 32)   /* one padding block of ciphertext */

static const char B64U[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static char *dup_cstr(const char *s)
{
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *err_new(const char *msg)
{
    return msg ? dup_cstr(msg) : NULL;
}

/* ── base64url ──────────────────────────────────────────────────────────────── */

static char *b64u_encode(const unsigned char *data, size_t len)
{
    size_t olen = ((len + 2) / 3) * 4;
    char *out = malloc(olen + 1);
    if (!out) return NULL;
    size_t o = 0, i = 0;
    while (i + 2 < len) {
        unsigned v = ((unsigned)data[i] << 16) | ((unsigned)data[i + 1] << 8) | data[i + 2];
        out[o++] = B64U[(v >> 18) & 63];
        out[o++] = B64U[(v >> 12) & 63];
        out[o++] = B64U[(v >> 6) & 63];
        out[o++] = B64U[v & 63];
        i += 3;
    }
    if (i + 1 == len) {
        unsigned v = (unsigned)data[i] << 16;
        out[o++] = B64U[(v >> 18) & 63];
        out[o++] = B64U[(v >> 12) & 63];
        out[o++] = '=';
        out[o++] = '=';
    } else if (i + 2 == len) {
        unsigned v = ((unsigned)data[i] << 16) | ((unsigned)data[i + 1] << 8);
        out[o++] = B64U[(v >> 18) & 63];
        out[o++] = B64U[(v >> 12) & 63];
        out[o++] = B64U[(v >> 6) & 63];
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

static int b64u_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

/* Accepts padded and unpadded input; standard '+'/'/' are refused rather than silently accepted, so a
 * standard-base64 secret is an error instead of a working-but-different key. */
static unsigned char *b64u_decode(const char *in, size_t *out_len)
{
    if (!in) return NULL;
    size_t n = strlen(in);
    while (n && in[n - 1] == '=') n--;
    if (n % 4 == 1) return NULL;

    unsigned char *out = malloc((n / 4) * 3 + 3);
    if (!out) return NULL;
    size_t o = 0;
    unsigned acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        int v = b64u_val(in[i]);
        if (v < 0) { free(out); return NULL; }
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    *out_len = o;
    return out;
}

/* ── keys ───────────────────────────────────────────────────────────────────── */

int fernet_key_split(const char *secret,
                     unsigned char signing[16], unsigned char encryption[16])
{
    size_t len = 0;
    unsigned char *raw = b64u_decode(secret, &len);
    if (!raw) return -1;
    if (len != 32) { free(raw); return -1; }
    memcpy(signing, raw, 16);
    memcpy(encryption, raw + 16, 16);
    free(raw);
    return 0;
}

/* ── AES-128-CBC with PKCS#7 ────────────────────────────────────────────────── */

static unsigned char *aes_cbc(const int encrypt, const unsigned char key[16],
                              const unsigned char iv[FERNET_IV_LEN],
                              const unsigned char *in, size_t len, size_t *out_len)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return NULL;
    unsigned char *out = malloc(len + EVP_MAX_BLOCK_LENGTH);
    if (!out) { EVP_CIPHER_CTX_free(ctx); return NULL; }

    int n1 = 0, n2 = 0;
    int ok = encrypt ? EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv)
                     : EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
    if (ok != 1) goto fail;
    ok = encrypt ? EVP_EncryptUpdate(ctx, out, &n1, in, (int)len)
                 : EVP_DecryptUpdate(ctx, out, &n1, in, (int)len);
    if (ok != 1) goto fail;
    ok = encrypt ? EVP_EncryptFinal_ex(ctx, out + n1, &n2)
                 : EVP_DecryptFinal_ex(ctx, out + n1, &n2);
    if (ok != 1) goto fail;                       /* padding failure lands here */

    EVP_CIPHER_CTX_free(ctx);
    *out_len = (size_t)(n1 + n2);
    return out;

fail:
    EVP_CIPHER_CTX_free(ctx);
    free(out);
    return NULL;
}

static void put_u64_be(unsigned char *p, unsigned long long v)
{
    for (int i = 7; i >= 0; i--) { p[i] = (unsigned char)(v & 0xFF); v >>= 8; }
}

static unsigned long long get_u64_be(const unsigned char *p)
{
    unsigned long long v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}

/* ── encrypt ────────────────────────────────────────────────────────────────── */

char *fernet_encrypt(const char *secret, const unsigned char *plain, size_t len,
                     const unsigned char iv[FERNET_IV_LEN], long long ts, char **err)
{
    if (err) *err = NULL;
    unsigned char signing[16], enc[16];
    if (fernet_key_split(secret, signing, enc) != 0) {
        if (err) *err = err_new("invalid key");
        return NULL;
    }

    size_t ct_len = 0;
    unsigned char *ct = aes_cbc(1, enc, iv, plain, len, &ct_len);
    if (!ct) {
        if (err) *err = err_new("encrypt failed");
        return NULL;
    }

    size_t body = 1 + 8 + FERNET_IV_LEN + ct_len;      /* everything the HMAC covers */
    unsigned char *buf = malloc(body + 32);
    if (!buf) { free(ct); if (err) *err = err_new("out of memory"); return NULL; }
    buf[0] = FERNET_VERSION;
    put_u64_be(buf + 1, (unsigned long long)ts);
    memcpy(buf + 9, iv, FERNET_IV_LEN);
    memcpy(buf + 9 + FERNET_IV_LEN, ct, ct_len);
    free(ct);

    crypto_hmac_sha256(signing, 16, buf, body, buf + body);

    char *token = b64u_encode(buf, body + 32);
    free(buf);
    if (!token && err) *err = err_new("out of memory");
    return token;
}

/* ── decrypt ────────────────────────────────────────────────────────────────── */

unsigned char *fernet_decrypt(const char *secret, const char *token,
                              long long now, long long ttl, size_t *out_len, char **err)
{
    if (err) *err = NULL;
    if (out_len) *out_len = 0;

    unsigned char signing[16], enc[16];
    if (fernet_key_split(secret, signing, enc) != 0) {
        if (err) *err = err_new("invalid key");
        return NULL;
    }

    size_t raw_len = 0;
    unsigned char *raw = b64u_decode(token, &raw_len);
    if (!raw) {                                  /* also covers "invalid base64" */
        if (err) *err = err_new("invalid token");
        return NULL;
    }
    if (raw_len < FERNET_MIN_LEN || (raw_len - 1 - 8 - FERNET_IV_LEN - 32) % 16 != 0) {
        free(raw);
        if (err) *err = err_new("invalid token");
        return NULL;
    }
    if (raw[0] != FERNET_VERSION) {
        free(raw);
        if (err) *err = err_new("invalid token");
        return NULL;
    }

    size_t body = raw_len - 32;
    unsigned char mac[32];
    crypto_hmac_sha256(signing, 16, raw, body, mac);

    /* Constant-time compare: a timing oracle here would let a forger walk the MAC out one byte at a
     * time, which is the whole reason the MAC exists. */
    unsigned diff = 0;
    for (int i = 0; i < 32; i++) diff |= (unsigned)(mac[i] ^ raw[body + i]);
    if (diff != 0) {
        free(raw);
        if (err) *err = err_new("invalid token");
        return NULL;
    }

    if (ttl >= 0) {
        /* cryptography compares the absolute difference, so a token stamped in the future is refused
         * as well -- a clock that jumped backwards must not make an old token look fresh. */
        long long ts = (long long)get_u64_be(raw + 1);
        long long diff = now - ts;
        if (diff < 0) diff = -diff;
        if (diff > ttl) {
            free(raw);
            if (err) *err = err_new("expired");
            return NULL;
        }
    }

    size_t ct_len = raw_len - 32 - (1 + 8 + FERNET_IV_LEN);
    size_t plain_len = 0;
    unsigned char *plain = aes_cbc(0, enc, raw + 9, raw + 9 + FERNET_IV_LEN, ct_len, &plain_len);
    free(raw);
    if (!plain) {                                 /* padding error, or a wrong IV */
        if (err) *err = err_new("invalid token");
        return NULL;
    }
    if (out_len) *out_len = plain_len;
    return plain;
}

char *fernet_generate_key(void)
{
    unsigned char raw[32];
    if (!crypto_random_bytes(raw, sizeof raw)) return NULL;
    return b64u_encode(raw, sizeof raw);
}
