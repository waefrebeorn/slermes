/*
 * weixin_crypto.c - weixin AES-128-ECB / account-id helpers.
 *
 * Split from weixin.c (soul.md: self-contained modules). PKCS7 padding,
 * AES-128-ECB encrypt/decrypt, key parsing and account-id helpers.
 * No file-scope state.
 */
#include "weixin_crypto.h"
#include "hermes_json.h"   /* json_t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "libbase64/base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>



/* ===========================================================================
 *  WeChat crypto helpers — ported from gateway/platforms/weixin.py
 *  Real AES-128-ECB (PKCS7) via OpenSSL EVP. These were REAL_GAP.
 * =========================================================================== */

/* PoP: _pkcs7_pad @ plugins/platforms/weixin/adapter.py:_pkcs7_pad */
/* Caller frees the returned buffer. Returns NULL on alloc failure. */
/* PoP: weixin_pkcs7_pad @ plugins/platforms/weixin/adapter.py:_pkcs7_pad */
unsigned char *weixin_pkcs7_pad(const unsigned char *data, size_t len, size_t *out_len)
{
    size_t pad = 16 - (len % 16);
    size_t total = len + pad;
    unsigned char *buf = (unsigned char *)malloc(total ? total : 1);
    if (!buf) return NULL;
    if (len) memcpy(buf, data, len);
    memset(buf + len, (int)pad, pad);
    *out_len = total;
    return buf;
}


/* PoP: _aes_padded_size @ gateway/platforms/weixin.py:_aes_padded_size */
size_t weixin_aes_padded_size(size_t size)
{
    return ((size + 1 + 15) / 16) * 16;
}


/* PoP: _aes128_ecb_encrypt @ plugins/platforms/weixin/adapter.py:_aes128_ecb_encrypt */
/* Encrypts PKCS7-padded plaintext with AES-128-ECB. Caller frees *out. */
/* PoP: weixin_aes128_ecb_encrypt @ plugins/platforms/weixin/adapter.py:_aes128_ecb_encrypt */
int weixin_aes128_ecb_encrypt(const unsigned char *plaintext, size_t pt_len,
                              const unsigned char *key, size_t key_len,
                              unsigned char **out, size_t *out_len)
{
    if (!plaintext || !key || key_len != 16 || !out) return -1;
    size_t pad_len = 0;
    unsigned char *padded = weixin_pkcs7_pad(plaintext, pt_len, &pad_len);
    if (!padded) return -1;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { free(padded); return -1; }
    unsigned char *ct = (unsigned char *)malloc(pad_len + 16);
    if (!ct) { EVP_CIPHER_CTX_free(ctx); free(padded); return -1; }
    int len1 = 0, len2 = 0;
    EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
    EVP_CIPHER_CTX_set_padding(ctx, 0); /* manual PKCS7 */
    EVP_EncryptUpdate(ctx, ct, &len1, padded, (int)pad_len);
    EVP_EncryptFinal_ex(ctx, ct + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);
    free(padded);

    *out = ct;
    *out_len = (size_t)(len1 + len2);
    return 0;
}


/* PoP: _aes128_ecb_decrypt @ plugins/platforms/weixin/adapter.py:_aes128_ecb_decrypt */
/* Decrypts AES-128-ECB and strips PKCS7 padding. Caller frees *out. */
/* PoP: weixin_aes128_ecb_decrypt @ plugins/platforms/weixin/adapter.py:_aes128_ecb_decrypt */
int weixin_aes128_ecb_decrypt(const unsigned char *ciphertext, size_t ct_len,
                              const unsigned char *key, size_t key_len,
                              unsigned char **out, size_t *out_len)
{
    if (!ciphertext || !key || key_len != 16 || !out) return -1;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;
    unsigned char *pt = (unsigned char *)malloc(ct_len + 16);
    if (!pt) { EVP_CIPHER_CTX_free(ctx); return -1; }
    int len1 = 0, len2 = 0;
    EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
    EVP_CIPHER_CTX_set_padding(ctx, 0); /* manual PKCS7 */
    EVP_DecryptUpdate(ctx, pt, &len1, ciphertext, (int)ct_len);
    EVP_DecryptFinal_ex(ctx, pt + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);

    size_t total = (size_t)(len1 + len2);
    if (total > 0) {
        unsigned char pad = pt[total - 1];
        if (pad >= 1 && pad <= 16) {
            int ok = 1;
            for (size_t i = total - pad; i < total; i++) if (pt[i] != pad) ok = 0;
            if (ok) total -= pad;
        }
    }
    *out = pt;
    *out_len = total;
    return 0;
}


/* PoP: _safe_id @ plugins/platforms/weixin/adapter.py:_safe_id */
/* Strip non-printable / non-identifier chars for safe logging. */
/* PoP: weixin_safe_id @ plugins/platforms/weixin/adapter.py:_safe_id */
void weixin_safe_id(const char *in, char *out, size_t out_sz)
{
    size_t o = 0;
    if (in) for (size_t i = 0; in[i] && o + 1 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '_' || c == '-' || c == '@' || c == '.' ||
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            out[o++] = (char)c;
    }
    out[o] = '\0';
}


/* PoP: _random_wechat_uin @ gateway/platforms/weixin.py:_random_wechat_uin */
/* Generate a random 32-bit big-endian value, base64-encoded as ASCII. */
/* PoP: weixin_random_wechat_uin @ gateway/platforms/weixin.py:_random_wechat_uin */
void weixin_random_wechat_uin(char *out, size_t out_sz)
{
    unsigned char b[4];
    if (RAND_bytes(b, 4) != 1) { if (out_sz) out[0]='\0'; return; }
    unsigned long v = ((unsigned long)b[0] << 24) | ((unsigned long)b[1] << 16) |
                      ((unsigned long)b[2] << 8) | b[3];
    char s[24];
    snprintf(s, sizeof(s), "%lu", v);
    char *enc = base64_encode((const unsigned char *)s, strlen(s));
    if (enc) { snprintf(out, out_sz, "%s", enc); free(enc); }
    else if (out_sz) out[0] = '\0';
}


/* ── Simple utility ports ───────────────────────────────────────────── */

/* PoP: _check_weixin_requirements @ plugins/platforms/weixin/adapter.py:check_weixin_requirements */
/* PoP: weixin_check_requirements @ plugins/platforms/weixin/adapter.py:check_weixin_requirements */
bool weixin_check_requirements(void)
{
    return true; /* C port: always available in compiled binary */
}


/* PoP: _base_info @ gateway/platforms/weixin.py:_base_info */
/* PoP: weixin_base_info @ gateway/platforms/weixin.py:_base_info */
json_t *weixin_base_info(void)
{
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_object_set(obj, "channel_version", json_string("2.0"));
    return obj;
}


/* PoP: _account_dir @ plugins/platforms/weixin/adapter.py:_account_dir */
/* PoP: weixin_account_dir @ plugins/platforms/weixin/adapter.py:_account_dir */
char *weixin_account_dir(const char *hermes_home)
{
    if (!hermes_home) return NULL;
    size_t sz = strlen(hermes_home) + 32;
    char *path = (char *)malloc(sz);
    if (!path) return NULL;
    snprintf(path, sz, "%s/weixin/accounts", hermes_home);
    mkdir(path, 0700);
    return path;
}


/* PoP: _account_file @ gateway/platforms/weixin.py:_account_file */
/* PoP: weixin_account_file @ gateway/platforms/weixin.py:_account_file */
char *weixin_account_file(const char *hermes_home, const char *account_id)
{
    if (!hermes_home || !account_id) return NULL;
    size_t sz = strlen(hermes_home) + strlen(account_id) + 64;
    char *path = (char *)malloc(sz);
    if (!path) return NULL;
    char *dir = weixin_account_dir(hermes_home);
    if (!dir) { free(path); return NULL; }
    snprintf(path, sz, "%s/%s.json", dir, account_id);
    free(dir);
    return path;
}


/* PoP: _parse_aes_key @ plugins/platforms/weixin/adapter.py:_parse_aes_key */
/* Decode a base64 AES key; accept 16 raw bytes or a 32-char hex string. */
int weixin_parse_aes_key(const char *aes_key_b64, unsigned char *out, size_t *out_len)
{
    if (!aes_key_b64 || !out || !out_len) return -1;
    size_t dlen = 0;
    unsigned char *dec = base64_decode(aes_key_b64, &dlen);
    if (!dec) return -1;
    if (dlen == 16) {
        memcpy(out, dec, 16);
        *out_len = 16;
        free(dec);
        return 0;
    }
    if (dlen == 32) {
        /* If it decodes to a pure hex string, interpret as hex bytes. */
        bool all_hex = true;
        for (size_t i = 0; i < 32; i++) {
            char c = (char)dec[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                all_hex = false; break;
            }
        }
        if (all_hex) {
            for (size_t i = 0; i < 16; i++) {
                unsigned int b = 0;
                sscanf((char *)dec + i * 2, "%2x", &b);
                out[i] = (unsigned char)b;
            }
            *out_len = 16;
            free(dec);
            return 0;
        }
    }
    free(dec);
    return -1; /* unexpected aes_key format */
}
