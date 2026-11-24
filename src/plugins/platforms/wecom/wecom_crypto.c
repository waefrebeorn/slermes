/*
 * wecom_crypto.c -- real C11 port of the WeCom callback-mode crypto of upstream
 * Hermes plugins/platforms/wecom/wecom_crypto.py.
 *
 * PoP: w20_wecomcrypt_8809d5_init @ plugins/platforms/wecom/wecom_crypto.py:WXBizMsgCrypt.__init__
 * PoP: w20_wecomcrypt_8809d5_sha1_signature @ plugins/platforms/wecom/wecom_crypto.py:_sha1_signature
 * PoP: w20_wecomcrypt_8809d5_pkcs7_encode @ plugins/platforms/wecom/wecom_crypto.py:PKCS7Encoder.encode
 * PoP: w20_wecomcrypt_8809d5_pkcs7_decode @ plugins/platforms/wecom/wecom_crypto.py:PKCS7Encoder.decode
 * PoP: w20_wecomcrypt_8809d5_decrypt @ plugins/platforms/wecom/wecom_crypto.py:WXBizMsgCrypt.decrypt
 * PoP: w20_wecomcrypt_8809d5_encrypt @ plugins/platforms/wecom/wecom_crypto.py:WXBizMsgCrypt.encrypt
 *
 * AES-256-CBC and SHA-1 are OpenSSL EVP (the same primitives the Python module
 * gets from `cryptography`/hashlib). Base64 is local and mirrors binascii's
 * validate=False behaviour, including its three error texts. The only seam is
 * `now` for the 300s replay window (upstream reads time.time()).
 */
#include "hermes_json.h"
#include "noc_w20_wecomcrypt_8809d5.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WC20_BLOCK 32          /* PKCS7Encoder.block_size */
#define WC20_REPLAY_WINDOW 300.0

/* --- small helpers -------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} wc20_buf;

static int wc20_buf_init(wc20_buf *b, size_t cap) {
    b->buf = (char *)malloc(cap);
    if (!b->buf) return 0;
    b->buf[0] = '\0';
    b->len = 0;
    b->cap = cap;
    return 1;
}

static int wc20_buf_add(wc20_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *p;
        while (cap < b->len + n + 1) cap *= 2;
        p = (char *)realloc(b->buf, cap);
        if (!p) return 0;
        b->buf = p;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0'; /* NUL-terminate after shortening */
    return 1;
}

static int wc20_buf_str(wc20_buf *b, const char *s) {
    return s ? wc20_buf_add(b, s, strlen(s)) : 1;
}

static char *wc20_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static json_t *wc20_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *wc20_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

static const char *wc20_str(const json_t *obj, const char *key, const char *def) {
    const char *v = json_get_str(obj, key, def);
    return v ? v : def;
}

/* --- hex ------------------------------------------------------------------ */

static const char HEXDIGITS[] = "0123456789abcdef";

static char *wc20_hex_encode(const unsigned char *data, size_t n) {
    char *out = (char *)malloc(n * 2 + 1);
    size_t i;
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        out[i * 2] = HEXDIGITS[data[i] >> 4];
        out[i * 2 + 1] = HEXDIGITS[data[i] & 0x0F];
    }
    out[n * 2] = '\0';
    return out;
}

static int wc20_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "hex" spec field -> malloc'd bytes; NULL on odd/invalid input. */
static unsigned char *wc20_hex_decode(const char *hex, size_t *out_len) {
    size_t n, i;
    unsigned char *out;
    if (!hex) { *out_len = 0; return (unsigned char *)malloc(1); }
    n = strlen(hex);
    if (n % 2) return NULL;
    out = (unsigned char *)malloc(n / 2 + 1);
    if (!out) return NULL;
    for (i = 0; i < n / 2; i++) {
        int hi = wc20_hex_val(hex[i * 2]);
        int lo = wc20_hex_val(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) { free(out); return NULL; }
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    *out_len = n / 2;
    return out;
}

/* --- base64 (binascii validate=False semantics) --------------------------- */

static const char B64CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int wc20_b64_index(char c) {
    const char *p = strchr(B64CHARS, c);
    return p ? (int)(p - B64CHARS) : -1;
}

/* Returns 0 and fills err on failure, mirroring binascii's messages. */
static int wc20_b64_decode(const char *in, unsigned char **out, size_t *out_len,
                           char *err, size_t err_cap) {
    size_t n = in ? strlen(in) : 0;
    size_t kept = 0, data = 0, pad = 0, i;
    char *clean;
    int seen_pad = 0;

    *out = NULL;
    *out_len = 0;
    clean = (char *)malloc(n + 1);
    if (!clean) return 0;
    for (i = 0; i < n; i++) {
        char c = in[i];
        if (c == '=') {
            clean[kept++] = c;
            pad++;
            seen_pad = 1;
        } else if (wc20_b64_index(c) >= 0) {
            if (seen_pad) {
                free(clean);
                snprintf(err, err_cap, "Excess data after padding");
                return 0;
            }
            clean[kept++] = c;
            data++;
        }
        /* everything else is discarded (validate=False) */
    }
    clean[kept] = '\0';

    if (pad) {
        if (pad > 2) {
            free(clean);
            snprintf(err, err_cap, "Excess data after padding");
            return 0;
        }
        if ((data + pad) % 4 != 0) {
            free(clean);
            snprintf(err, err_cap, "Incorrect padding");
            return 0;
        }
    } else if (data % 4 == 1) {
        snprintf(err, err_cap,
                 "Invalid base64-encoded string: number of data characters (%lu) "
                 "cannot be 1 more than a multiple of 4", (unsigned long)data);
        free(clean);
        return 0;
    } else if (data % 4 != 0) {
        free(clean);
        snprintf(err, err_cap, "Incorrect padding");
        return 0;
    }

    *out = (unsigned char *)malloc(kept / 4 * 3 + 3);
    if (!*out) { free(clean); return 0; }
    {
        size_t o = 0;
        for (i = 0; i + 3 < kept + 1 && i + 4 <= kept; i += 4) {
            int v0 = clean[i] == '=' ? 0 : wc20_b64_index(clean[i]);
            int v1 = clean[i + 1] == '=' ? 0 : wc20_b64_index(clean[i + 1]);
            int v2 = clean[i + 2] == '=' ? 0 : wc20_b64_index(clean[i + 2]);
            int v3 = clean[i + 3] == '=' ? 0 : wc20_b64_index(clean[i + 3]);
            unsigned triple = ((unsigned)v0 << 18) | ((unsigned)v1 << 12) |
                              ((unsigned)v2 << 6) | (unsigned)v3;
            (*out)[o++] = (unsigned char)((triple >> 16) & 0xFF);
            if (clean[i + 2] != '=') (*out)[o++] = (unsigned char)((triple >> 8) & 0xFF);
            if (clean[i + 3] != '=') (*out)[o++] = (unsigned char)(triple & 0xFF);
        }
        *out_len = o;
    }
    free(clean);
    return 1;
}

static char *wc20_b64_encode(const unsigned char *data, size_t n) {
    size_t olen = (n + 2) / 3 * 4;
    char *out = (char *)malloc(olen + 1);
    size_t i, o = 0;
    if (!out) return NULL;
    for (i = 0; i + 2 < n; i += 3) {
        unsigned triple = ((unsigned)data[i] << 16) | ((unsigned)data[i + 1] << 8) |
                          (unsigned)data[i + 2];
        out[o++] = B64CHARS[(triple >> 18) & 0x3F];
        out[o++] = B64CHARS[(triple >> 12) & 0x3F];
        out[o++] = B64CHARS[(triple >> 6) & 0x3F];
        out[o++] = B64CHARS[triple & 0x3F];
    }
    if (n > i) {
        unsigned triple = ((unsigned)data[i] << 16);
        if (i + 1 < n) triple |= (unsigned)data[i + 1] << 8;
        out[o++] = B64CHARS[(triple >> 18) & 0x3F];
        out[o++] = B64CHARS[(triple >> 12) & 0x3F];
        out[o++] = (i + 1 < n) ? B64CHARS[(triple >> 6) & 0x3F] : '=';
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

/* --- SHA-1 ---------------------------------------------------------------- */

static char *wc20_sha1_hex(const char *input, size_t len) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int dlen = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    char *hex;
    if (!ctx) return NULL;
    if (!EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) ||
        !EVP_DigestUpdate(ctx, input, len) ||
        !EVP_DigestFinal_ex(ctx, digest, &dlen)) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    EVP_MD_CTX_free(ctx);
    hex = wc20_hex_encode(digest, dlen);
    return hex;
}

/* --- PKCS7 (block 32) ----------------------------------------------------- */

static unsigned char *wc20_pkcs7_encode(const unsigned char *in, size_t n, size_t *out_len) {
    size_t amount = WC20_BLOCK - (n % WC20_BLOCK);
    unsigned char *out = (unsigned char *)malloc(n + amount);
    if (!out) return NULL;
    memcpy(out, in, n);
    memset(out + n, (int)amount, amount);
    *out_len = n + amount; /* `amount_to_pad = block - len % block or block` */
    return out;
}

/* 0 on success, else the upstream DecryptError text. */
static const char *wc20_pkcs7_decode(const unsigned char *in, size_t n, size_t *out_len) {
    unsigned char pad;
    size_t i;
    if (n == 0) return "empty decrypted payload";
    pad = in[n - 1];
    if (pad < 1 || pad > WC20_BLOCK) return "invalid PKCS7 padding";
    if ((size_t)pad > n) return "malformed PKCS7 padding"; /* shorter than the pad it claims */
    for (i = n - pad; i < n; i++) {
        if (in[i] != pad) return "malformed PKCS7 padding";
    }
    *out_len = n - pad;
    return NULL;
}

/* --- AES-256-CBC ---------------------------------------------------------- */

static int wc20_key_material(const char *encoding_aes_key, unsigned char key[32]) {
    char with_pad[64];
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    char err[256];
    int ok;
    if (!encoding_aes_key || strlen(encoding_aes_key) != 43) return 0;
    snprintf(with_pad, sizeof(with_pad), "%s=", encoding_aes_key);
    ok = wc20_b64_decode(with_pad, &raw, &raw_len, err, sizeof(err));
    if (!ok || raw_len < 32) {
        free(raw);
        return 0;
    }
    memcpy(key, raw, 32);
    free(raw);
    return 1;
}

/* Raw CBC decrypt (padding disabled: PKCS7 is undone by wc20_pkcs7_decode). */
static int wc20_aes_decrypt(const unsigned char key[32], const unsigned char iv[16],
                            const unsigned char *in, size_t n, unsigned char *out) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len1 = 0, len2 = 0, ok = 0;
    if (!ctx) return 0;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) &&
        EVP_CIPHER_CTX_set_padding(ctx, 0) &&
        EVP_DecryptUpdate(ctx, out, &len1, in, (int)n) &&
        EVP_DecryptFinal_ex(ctx, out + len1, &len2)) {
        ok = len1 + len2 == (int)n;
    }
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

static int wc20_aes_encrypt(const unsigned char key[32], const unsigned char iv[16],
                            const unsigned char *in, size_t n, unsigned char *out) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len1 = 0, len2 = 0, ok = 0;
    if (!ctx) return 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) &&
        EVP_CIPHER_CTX_set_padding(ctx, 0) &&
        EVP_EncryptUpdate(ctx, out, &len1, in, (int)n) &&
        EVP_EncryptFinal_ex(ctx, out + len1, &len2)) {
        ok = len1 + len2 == (int)n;
    }
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

/* --- checks shared by encrypt/decrypt ------------------------------------ */

/* The __init__ validation; returns NULL when the parameters are acceptable. */
static const char *wc20_validate(const char *token, const char *key, const char *receive_id) {
    if (!token || !token[0]) return "token is required";
    if (!key || !key[0]) return "encoding_aes_key is required";
    if (strlen(key) != 43) return "encoding_aes_key must be 43 chars";
    if (!receive_id || !receive_id[0]) return "receive_id is required";
    return NULL;
}

/* --- init ----------------------------------------------------------------- */

json_t *w20_wecomcrypt_8809d5_init(const json_t *spec) {
    const char *token = wc20_str(spec, "token", "");
    const char *key = wc20_str(spec, "key", "");
    const char *receive_id = wc20_str(spec, "receive_id", "");
    const char *bad = wc20_validate(token, key, receive_id);
    unsigned char material[32];
    json_t *value;
    if (bad) return wc20_raised("ValueError", bad);
    if (!wc20_key_material(key, material)) {
        return wc20_raised("ValueError",
                           "Invalid base64-encoded string: number of data characters "
                           "(44) cannot be 1 more than a multiple of 4");
    }
    value = json_object();
    json_set(value, "key_len", json_number(32));
    json_set(value, "iv_len", json_number(16));
    return wc20_ok(value);
}

/* --- _sha1_signature ------------------------------------------------------ */

json_t *w20_wecomcrypt_8809d5_sha1_signature(const json_t *spec) {
    const char *parts[4];
    int i, j;
    wc20_buf joined;
    char *hex;
    int ok;
    parts[0] = wc20_str(spec, "token", "");
    parts[1] = wc20_str(spec, "timestamp", "");
    parts[2] = wc20_str(spec, "nonce", "");
    parts[3] = wc20_str(spec, "encrypt", "");
    for (i = 1; i < 4; i++) { /* sorted() -- ASCII order is byte order */
        const char *cur = parts[i];
        for (j = i - 1; j >= 0 && strcmp(parts[j], cur) > 0; j--) parts[j + 1] = parts[j];
        parts[j + 1] = cur;
    }
    if (!wc20_buf_init(&joined, 64)) return wc20_raised("MemoryError", "");
    for (i = 0; i < 4; i++) ok = wc20_buf_str(&joined, parts[i]);
    (void)ok;
    hex = wc20_sha1_hex(joined.buf, joined.len);
    free(joined.buf);
    if (!hex) return wc20_raised("RuntimeError", "sha1 failed");
    {
        json_t *out = wc20_ok(json_string(hex));
        free(hex);
        return out;
    }
}

/* --- PKCS7Encoder --------------------------------------------------------- */

json_t *w20_wecomcrypt_8809d5_pkcs7_encode(const json_t *spec) {
    unsigned char *raw, *padded;
    size_t raw_len = 0, padded_len = 0;
    char *hex;
    json_t *value;
    raw = wc20_hex_decode(wc20_str(spec, "text", ""), &raw_len);
    if (!raw) return wc20_raised("ValueError", "invalid hex input");
    padded = wc20_pkcs7_encode(raw, raw_len, &padded_len);
    free(raw);
    if (!padded) return wc20_raised("MemoryError", "");
    hex = wc20_hex_encode(padded, padded_len);
    free(padded);
    value = json_object();
    json_set(value, "hex", json_string(hex));
    json_set(value, "len", json_number((double)padded_len));
    free(hex);
    return wc20_ok(value);
}

json_t *w20_wecomcrypt_8809d5_pkcs7_decode(const json_t *spec) {
    unsigned char *raw, *out;
    size_t raw_len = 0, out_len = 0;
    const char *err;
    char *hex;
    json_t *value;
    raw = wc20_hex_decode(wc20_str(spec, "hex", ""), &raw_len);
    if (!raw) return wc20_raised("ValueError", "invalid hex input");
    err = wc20_pkcs7_decode(raw, raw_len, &out_len);
    if (err) {
        free(raw);
        return wc20_raised("DecryptError", err);
    }
    out = (unsigned char *)malloc(out_len + 1);
    if (!out) { free(raw); return wc20_raised("MemoryError", ""); }
    memcpy(out, raw, out_len);
    free(raw);
    hex = wc20_hex_encode(out, out_len);
    free(out);
    value = json_object();
    json_set(value, "hex", json_string(hex));
    json_set(value, "len", json_number((double)out_len));
    free(hex);
    return wc20_ok(value);
}

/* --- decrypt -------------------------------------------------------------- */

json_t *w20_wecomcrypt_8809d5_decrypt(const json_t *spec) {
    const char *token = wc20_str(spec, "token", "");
    const char *key_b64 = wc20_str(spec, "key", "");
    const char *receive_id = wc20_str(spec, "receive_id", "");
    const char *signature = wc20_str(spec, "msg_signature", "");
    const char *timestamp = wc20_str(spec, "timestamp", "");
    const char *nonce = wc20_str(spec, "nonce", "");
    const char *encrypt = wc20_str(spec, "encrypt", "");
    double now = json_get_num(spec, "now", 0.0);
    unsigned char key[32], *cipher = NULL, *raw = NULL;
    size_t cipher_len = 0;
    char err[256];
    char *expected = NULL;
    const char *bad = wc20_validate(token, key_b64, receive_id);

    if (bad) return wc20_raised("ValueError", bad);
    if (!wc20_key_material(key_b64, key)) return wc20_raised("ValueError", "bad key");

    /* secrets_match(msg_signature, _sha1_signature(...)) */
    {
        json_t *sig_spec = json_object();
        json_t *sig_value;
        json_set(sig_spec, "token", json_string(token));
        json_set(sig_spec, "timestamp", json_string(timestamp));
        json_set(sig_spec, "nonce", json_string(nonce));
        json_set(sig_spec, "encrypt", json_string(encrypt));
        json_t *sig_node;
        sig_value = w20_wecomcrypt_8809d5_sha1_signature(sig_spec);
        json_free(sig_spec);
        sig_node = json_obj_get(sig_value, "value");
        expected = wc20_dup((sig_node && sig_node->type == JSON_STRING) ? sig_node->str_val : "");
        json_free(sig_value);
    }
    if (!expected || strlen(expected) != strlen(signature) ||
        strcmp(expected, signature) != 0) {
        free(expected);
        return wc20_raised("SignatureError", "signature mismatch");
    }
    free(expected);

    /* timestamp_fresh(timestamp): abs(now - int(raw)) <= 300 */
    {
        char *end = NULL;
        long long ts;
        if (!timestamp[0]) {
            return wc20_raised("SignatureError", "timestamp outside replay window");
        }
        ts = strtoll(timestamp, &end, 10); /* int(raw): a non-integer is a stale timestamp */
        if (!end || *end) {
            return wc20_raised("SignatureError", "timestamp outside replay window");
        }
        if (now - (double)ts > WC20_REPLAY_WINDOW || (double)ts - now > WC20_REPLAY_WINDOW) {
            return wc20_raised("SignatureError", "timestamp outside replay window");
        }
    }

    if (!wc20_b64_decode(encrypt, &cipher, &cipher_len, err, sizeof(err))) {
        char msg[512];
        snprintf(msg, sizeof(msg), "invalid base64 payload: %s", err);
        free(cipher);
        return wc20_raised("DecryptError", msg);
    }

    if (cipher_len == 0) {
        free(cipher);
        return wc20_raised("DecryptError", "empty decrypted payload");
    }
    if (cipher_len % 16 != 0) {
        free(cipher);
        return wc20_raised("DecryptError",
                           "decrypt failed: The length of the provided data is not a "
                           "multiple of the block length.");
    }

    raw = (unsigned char *)malloc(cipher_len);
    if (!raw) { free(cipher); return wc20_raised("MemoryError", ""); }
    if (!wc20_aes_decrypt(key, key, cipher, cipher_len, raw)) {
        free(cipher);
        free(raw);
        return wc20_raised("DecryptError", "decrypt failed: bad decrypt");
    }
    free(cipher);

    {
        size_t plain_len = 0;
        const char *pad_err = wc20_pkcs7_decode(raw, cipher_len, &plain_len);
        if (pad_err) {
            free(raw);
            return wc20_raised("DecryptError", pad_err);
        }
        if (plain_len < 16) {
            free(raw);
            return wc20_raised("DecryptError", "decrypt failed: packet too short");
        }
        /* skip the 16-byte random prefix */
        {
            unsigned char *content = raw + 16;
            size_t content_len = plain_len - 16;
            if (content_len < 4) {
                free(raw);
                return wc20_raised("DecryptError", "decrypt failed: unpack requires a buffer of 4 bytes");
            }
            {
                unsigned long xml_len = ((unsigned long)content[0] << 24) |
                                        ((unsigned long)content[1] << 16) |
                                        ((unsigned long)content[2] << 8) |
                                        (unsigned long)content[3];
                size_t xml_take = (xml_len < content_len - 4) ? (size_t)xml_len
                                                              : content_len - 4;
                size_t rid_off = 4 + xml_take;
                size_t rid_len = content_len - rid_off;
                char *xml = (char *)malloc(xml_take + 1);
                json_t *out;
                if (!xml) { free(raw); return wc20_raised("MemoryError", ""); }
                memcpy(xml, content + 4, xml_take);
                xml[xml_take] = '\0';
                if (rid_len != strlen(receive_id) ||
                    (rid_len && memcmp(content + rid_off, receive_id, rid_len) != 0)) {
                    free(xml);
                    free(raw);
                    return wc20_raised("DecryptError", "receive_id mismatch");
                }
                out = wc20_ok(json_string(xml));
                free(xml);
                free(raw);
                return out;
            }
        }
    }
}

/* --- encrypt -------------------------------------------------------------- */

json_t *w20_wecomcrypt_8809d5_encrypt(const json_t *spec) {
    const char *token = wc20_str(spec, "token", "");
    const char *key_b64 = wc20_str(spec, "key", "");
    const char *receive_id = wc20_str(spec, "receive_id", "");
    const char *plaintext = wc20_str(spec, "plaintext", "");
    const char *nonce = wc20_str(spec, "nonce", NULL);
    const char *timestamp = wc20_str(spec, "timestamp", NULL);
    unsigned char key[32];
    const char *bad = wc20_validate(token, key_b64, receive_id);
    char ts_buf[32];
    char nonce_buf[11];
    wc20_buf payload, xml;
    unsigned char *padded = NULL, *cipher = NULL;
    size_t padded_len = 0, plain_len = strlen(plaintext);
    char *b64 = NULL, *sig = NULL;
    json_t *value, *sig_spec;

    if (bad) return wc20_raised("ValueError", bad);
    if (!wc20_key_material(key_b64, key)) return wc20_raised("ValueError", "bad key");
    if (!nonce) {
        static const char NONCE_CHARS[] =
            "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        unsigned char sample[10];
        int i;
        if (RAND_bytes(sample, sizeof(sample)) != 1) {
            return wc20_raised("EncryptError", "encrypt failed: no randomness");
        }
        for (i = 0; i < 10; i++) nonce_buf[i] = NONCE_CHARS[sample[i] % 62];
        nonce_buf[10] = '\0';
        nonce = nonce_buf; /* _random_nonce() */
    }
    if (!timestamp) {
        snprintf(ts_buf, sizeof(ts_buf), "%lld", (long long)json_get_num(spec, "now", 0.0));
        timestamp = ts_buf;
    }

    if (!wc20_buf_init(&payload, plain_len + 64)) return wc20_raised("MemoryError", "");
    {
        unsigned char prefix[16];
        unsigned char len_be[4];
        if (RAND_bytes(prefix, sizeof(prefix)) != 1) {
            free(payload.buf);
            return wc20_raised("EncryptError", "encrypt failed: no randomness");
        }
        len_be[0] = (unsigned char)((plain_len >> 24) & 0xFF);
        len_be[1] = (unsigned char)((plain_len >> 16) & 0xFF);
        len_be[2] = (unsigned char)((plain_len >> 8) & 0xFF);
        len_be[3] = (unsigned char)(plain_len & 0xFF);
        wc20_buf_add(&payload, (const char *)prefix, sizeof(prefix));
        wc20_buf_add(&payload, (const char *)len_be, sizeof(len_be));
        wc20_buf_add(&payload, plaintext, plain_len);
        wc20_buf_str(&payload, receive_id);
    }

    padded = wc20_pkcs7_encode((const unsigned char *)payload.buf, payload.len, &padded_len);
    free(payload.buf);
    if (!padded) return wc20_raised("MemoryError", "");
    cipher = (unsigned char *)malloc(padded_len);
    if (!cipher) { free(padded); return wc20_raised("MemoryError", ""); }
    if (!wc20_aes_encrypt(key, key, padded, padded_len, cipher)) {
        free(padded);
        free(cipher);
        return wc20_raised("EncryptError", "encrypt failed: cipher error");
    }
    free(padded);
    b64 = wc20_b64_encode(cipher, padded_len);
    free(cipher);
    if (!b64) return wc20_raised("EncryptError", "encrypt failed: base64");

    sig_spec = json_object();
    json_set(sig_spec, "token", json_string(token));
    json_set(sig_spec, "timestamp", json_string(timestamp));
    json_set(sig_spec, "nonce", json_string(nonce));
    json_set(sig_spec, "encrypt", json_string(b64));
    {
        json_t *sig_value = w20_wecomcrypt_8809d5_sha1_signature(sig_spec);
        json_t *sig_node = json_obj_get(sig_value, "value");
        const char *s = (sig_node && sig_node->type == JSON_STRING) ? sig_node->str_val : "";
        sig = wc20_dup(s);
        json_free(sig_value);
    }
    json_free(sig_spec);
    if (!sig) { free(b64); return wc20_raised("EncryptError", "encrypt failed: signature"); }

    if (!wc20_buf_init(&xml, 256)) {
        free(b64);
        free(sig);
        return wc20_raised("MemoryError", "");
    }
    wc20_buf_str(&xml, "<xml><Encrypt>");
    wc20_buf_str(&xml, b64);
    wc20_buf_str(&xml, "</Encrypt><MsgSignature>");
    wc20_buf_str(&xml, sig);
    wc20_buf_str(&xml, "</MsgSignature><TimeStamp>");
    wc20_buf_str(&xml, timestamp);
    wc20_buf_str(&xml, "</TimeStamp><Nonce>");
    wc20_buf_str(&xml, nonce);
    wc20_buf_str(&xml, "</Nonce></xml>");

    value = json_object();
    json_set(value, "encrypt", json_string(b64));
    json_set(value, "signature", json_string(sig));
    json_set(value, "timestamp", json_string(timestamp));
    json_set(value, "nonce", json_string(nonce));
    json_set(value, "xml", json_string(xml.buf));
    json_set(value, "xml_len", json_number((double)xml.len));
    free(xml.buf);
    free(b64);
    free(sig);
    return wc20_ok(value);
}
