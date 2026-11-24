/*
 * crypto.c -- real C11 port of plugins/platforms/qqbot/crypto.py.
 *
 * PoP: w22_crypto_2e84d7_bind_key @ plugins/platforms/qqbot/crypto.py:generate_bind_key
 * PoP: w22_crypto_2e84d7_bind_key_shape @ plugins/platforms/qqbot/crypto.py:generate_bind_key
 * PoP: w22_crypto_2e84d7_b64_encode_hex @ plugins/platforms/qqbot/crypto.py:generate_bind_key
 * PoP: w22_crypto_2e84d7_b64_decode_len @ plugins/platforms/qqbot/crypto.py:decrypt_secret
 * PoP: w22_crypto_2e84d7_decrypt @ plugins/platforms/qqbot/crypto.py:decrypt_secret
 *
 * generate_bind_key draws 32 bytes from the OS RNG (the module's only seam)
 * and base64-encodes them. decrypt_secret base64-decodes the blob as
 * base64.b64decode does, splits IV(12) || ciphertext || tag(16) the way the
 * Python slices do, opens it with AES-GCM through OpenSSL EVP (the same
 * primitive `cryptography`'s AESGCM wraps) and decodes strict UTF-8.
 */
#include "hermes_json.h"
#include "noc_w22_crypto_2e84d7.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRYPT_KEY_LEN 32
#define CRYPT_IV_SLICE 12
#define CRYPT_TAG_LEN 16

static const char CRYPT_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int crypt_alpha_index(unsigned char c) {
    const char *p;
    if (c == '\0') return -1;
    p = strchr(CRYPT_ALPHABET, (int)c);
    if (!p) return -1;
    return (int)(p - CRYPT_ALPHABET);
}

/* --- base64 (Python base64.b64decode(validate=False) semantics) ---------- */

typedef struct {
    unsigned char *data;
    size_t len;
    /* -1 ok; otherwise the Python exception detail */
    const char *why;   /* NULL | "padding" | "length_mod1" */
} crypt_b64;

/* Decodes into a malloc'd buffer (never NULL on success, may be malloc(1)). */
static void crypt_b64_decode(const char *text, crypt_b64 *out) {
    size_t cap, o = 0, data_chars = 0;
    int prev_sig = 0, last_sig = 0, quad = 0;
    unsigned long acc = 0;
    const unsigned char *p = (const unsigned char *)(text ? text : "");

    out->data = NULL;
    out->len = 0;
    out->why = NULL;
    cap = strlen((const char *)p) + 4;
    out->data = (unsigned char *)malloc(cap ? cap : 1);
    if (!out->data) { out->why = "padding"; return; }

    for (; *p; p++) {
        int v = crypt_alpha_index(*p);
        if (v >= 0) {
            data_chars++;
            quad++;
            acc = (acc << 6) | (unsigned long)v;
            if (quad == 4) {
                out->data[o++] = (unsigned char)((acc >> 16) & 0xff);
                out->data[o++] = (unsigned char)((acc >> 8) & 0xff);
                out->data[o++] = (unsigned char)(acc & 0xff);
                quad = 0;
                acc = 0;
            }
            prev_sig = last_sig;
            last_sig = 'c';
        } else if (*p == '=') {
            prev_sig = last_sig;
            last_sig = '=';
        }
        /* every other character is discarded, as in non-strict b64decode */
    }

    switch (data_chars % 4) {
    case 1:
        out->why = "length_mod1";
        break;
    case 2:
        if (!(prev_sig == '=' && last_sig == '=')) out->why = "padding";
        break;
    case 3:
        if (last_sig != '=') out->why = "padding";
        break;
    default:
        break;
    }
    if (out->why) { free(out->data); out->data = NULL; out->len = 0; return; }

    /* Flush the partial quad: 2 data chars -> 1 byte, 3 -> 2 bytes. */
    if (quad == 2) {
        out->data[o++] = (unsigned char)((acc >> 4) & 0xff);
    } else if (quad == 3) {
        out->data[o++] = (unsigned char)((acc >> 10) & 0xff);
        out->data[o++] = (unsigned char)((acc >> 2) & 0xff);
    }
    out->len = o;
}

static char *crypt_b64_encode(const unsigned char *data, size_t len) {
    size_t o = 0, i = 0;
    char *out = (char *)malloc(((len + 2) / 3) * 4 + 1);
    if (!out) return NULL;
    while (i + 2 < len) {
        unsigned a = data[i], b = data[i + 1], c = data[i + 2];
        out[o++] = CRYPT_ALPHABET[a >> 2];
        out[o++] = CRYPT_ALPHABET[((a & 3) << 4) | (b >> 4)];
        out[o++] = CRYPT_ALPHABET[((b & 15) << 2) | (c >> 6)];
        out[o++] = CRYPT_ALPHABET[c & 63];
        i += 3;
    }
    if (i < len) {
        unsigned a = data[i];
        out[o++] = CRYPT_ALPHABET[a >> 2];
        if (i + 1 < len) {
            unsigned b = data[i + 1];
            out[o++] = CRYPT_ALPHABET[((a & 3) << 4) | (b >> 4)];
            out[o++] = CRYPT_ALPHABET[(b & 15) << 2];
        } else {
            out[o++] = CRYPT_ALPHABET[(a & 3) << 4];
            out[o++] = '=';
        }
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

static int crypt_unhex(const char *hex, unsigned char **out, size_t *len) {
    size_t n, i;
    const char *p = hex ? hex : "";
    n = strlen(p);
    if (n % 2) return -1;
    *len = n / 2;
    *out = (unsigned char *)malloc(*len ? *len : 1);
    if (!*out) return -1;
    for (i = 0; i < *len; i++) {
        int hi, lo;
        char a = p[i * 2], b = p[i * 2 + 1];
        if (a >= '0' && a <= '9') hi = a - '0';
        else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
        else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
        else { free(*out); *out = NULL; return -1; }
        if (b >= '0' && b <= '9') lo = b - '0';
        else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
        else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
        else { free(*out); *out = NULL; return -1; }
        (*out)[i] = (unsigned char)((hi << 4) | lo);
    }
    return 0;
}

/* --- strict UTF-8 ------------------------------------------------------- */

static int crypt_utf8_valid(const unsigned char *s, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = s[i];
        unsigned long cp;
        int extra, k;
        if (c < 0x80) { i++; continue; }
        if ((c & 0xe0) == 0xc0) { cp = c & 0x1fUL; extra = 1; }
        else if ((c & 0xf0) == 0xe0) { cp = c & 0x0fUL; extra = 2; }
        else if ((c & 0xf8) == 0xf0) { cp = c & 0x07UL; extra = 3; }
        else return 0;
        if (i + (size_t)extra >= len) return 0;
        for (k = 1; k <= extra; k++) {
            if ((s[i + (size_t)k] & 0xc0) != 0x80) return 0;
            cp = (cp << 6) | (s[i + (size_t)k] & 0x3fUL);
        }
        /* overlong, surrogate, out-of-range */
        if (extra == 1 && cp < 0x80) return 0;
        if (extra == 2 && cp < 0x800) return 0;
        if (extra == 3 && cp < 0x10000) return 0;
        if (cp > 0x10ffffUL) return 0;
        if (cp >= 0xd800UL && cp <= 0xdfffUL) return 0;
        i += (size_t)extra + 1;
    }
    return 1;
}

/* --- AES-GCM ------------------------------------------------------------ */

static int crypt_gcm_open(const unsigned char *key, int key_len,
                          const unsigned char *iv, int iv_len,
                          const unsigned char *ct, int ct_len,
                          const unsigned char *tag, unsigned char *out) {
    EVP_CIPHER_CTX *ctx;
    const EVP_CIPHER *cipher;
    int ok = 0, outl = 0, total = 0;

    if (key_len == 32) cipher = EVP_aes_256_gcm();
    else if (key_len == 24) cipher = EVP_aes_192_gcm();
    else cipher = EVP_aes_128_gcm();

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;
    do {
        if (EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL) != 1) break;
        if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) break;
        if (ct_len > 0) {
            if (EVP_DecryptUpdate(ctx, out, &outl, ct, ct_len) != 1) break;
            total = outl;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, CRYPT_TAG_LEN,
                                (void *)tag) != 1) break;
        if (EVP_DecryptFinal_ex(ctx, out + total, &outl) != 1) break;
        total += outl;
        ok = 1;
    } while (0);
    EVP_CIPHER_CTX_free(ctx);
    return ok ? total : -1;
}

/* --- result helpers ----------------------------------------------------- */

static json_t *crypt_failed(const char *error, const char *why) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(error ? error : "ValueError"));
    if (why) json_set(out, "why", json_string(why));
    return out;
}

static json_t *crypt_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "value", value ? value : json_null());
    return out;
}

/* --- generate_bind_key -------------------------------------------------- */

char *w22_crypto_2e84d7_bind_key(void) {
    unsigned char key[CRYPT_KEY_LEN];
    if (RAND_bytes(key, (int)sizeof(key)) != 1) return NULL;
    return crypt_b64_encode(key, sizeof(key));
}

json_t *w22_crypto_2e84d7_bind_key_shape(void) {
    char *key = w22_crypto_2e84d7_bind_key();
    crypt_b64 decoded;
    json_t *out;
    size_t i, chars, padding = 0;
    int alphabet = 1;

    if (!key) return crypt_failed("RuntimeError", "rng");
    chars = strlen(key);
    for (i = 0; i < chars; i++) {
        if (key[i] == '=') padding++;
        else if (crypt_alpha_index((unsigned char)key[i]) < 0) alphabet = 0;
    }
    crypt_b64_decode(key, &decoded);
    out = json_object();
    json_set(out, "bytes", json_number(decoded.why ? -1.0 : (double)decoded.len));
    json_set(out, "chars", json_number((double)chars));
    json_set(out, "padding", json_number((double)padding));
    json_set(out, "alphabet", json_bool(alphabet));
    free(decoded.data);
    free(key);
    return out;
}

/* --- b64_encode_hex ----------------------------------------------------- */

json_t *w22_crypto_2e84d7_b64_encode_hex(const char *hex) {
    unsigned char *bytes = NULL;
    size_t len = 0;
    char *text;
    json_t *out;
    if (crypt_unhex(hex, &bytes, &len) != 0) return crypt_failed("ValueError", "hex");
    text = crypt_b64_encode(bytes, len);
    free(bytes);
    if (!text) return crypt_failed("ValueError", "hex");
    out = crypt_ok(json_string(text));
    free(text);
    return out;
}

/* --- b64_decode_len ----------------------------------------------------- */

json_t *w22_crypto_2e84d7_b64_decode_len(const char *text) {
    crypt_b64 decoded;
    json_t *out;
    crypt_b64_decode(text, &decoded);
    if (decoded.why) {
        json_t *err = crypt_failed("binascii.Error", decoded.why);
        return err;
    }
    out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "len", json_number((double)decoded.len));
    free(decoded.data);
    return out;
}

/* --- decrypt_secret ----------------------------------------------------- */

json_t *w22_crypto_2e84d7_decrypt(const char *encrypted_base64, const char *key_base64) {
    crypt_b64 raw, key;
    unsigned char *plain = NULL;
    size_t ct_len;
    int iv_len, rc;
    json_t *out;

    /* raw = base64.b64decode(encrypted_base64) */
    crypt_b64_decode(encrypted_base64, &raw);
    if (raw.why) return crypt_failed("binascii.Error", raw.why);

    /* AESGCM(base64.b64decode(key_base64)) */
    crypt_b64_decode(key_base64, &key);
    if (key.why) { free(raw.data); return crypt_failed("binascii.Error", key.why); }
    if (key.len != 16 && key.len != 24 && key.len != 32) {
        free(raw.data);
        free(key.data);
        return crypt_failed("ValueError", "key_length");
    }

    /* raw[:12] is the nonce, raw[12:] is ciphertext||tag */
    iv_len = raw.len < CRYPT_IV_SLICE ? (int)raw.len : CRYPT_IV_SLICE;
    if (iv_len < 8 || iv_len > 128) {
        free(raw.data);
        free(key.data);
        return crypt_failed("ValueError", "nonce_length");
    }
    if (raw.len < (size_t)iv_len + CRYPT_TAG_LEN) {
        free(raw.data);
        free(key.data);
        return crypt_failed("InvalidTag", "tag");
    }
    ct_len = raw.len - (size_t)iv_len - CRYPT_TAG_LEN;
    plain = (unsigned char *)malloc(ct_len + CRYPT_TAG_LEN + 1);
    if (!plain) {
        free(raw.data);
        free(key.data);
        return crypt_failed("InvalidTag", "tag");
    }
    rc = crypt_gcm_open(key.data, (int)key.len, raw.data, iv_len,
                        raw.data + iv_len, (int)ct_len,
                        raw.data + iv_len + ct_len, plain);
    free(raw.data);
    free(key.data);
    if (rc < 0) {
        free(plain);
        return crypt_failed("InvalidTag", "tag");
    }

    /* .decode("utf-8") */
    if (!crypt_utf8_valid(plain, (size_t)rc)) {
        free(plain);
        return crypt_failed("UnicodeDecodeError", "utf8");
    }
    plain[rc] = '\0';
    out = crypt_ok(json_string((const char *)plain));
    free(plain);
    return out;
}
