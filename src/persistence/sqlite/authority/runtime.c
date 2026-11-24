/*
 * runtime.c -- real C11 port of upstream Hermes
 *   persistence/sqlite/authority/runtime.py (module-level pure helpers).
 *
 * ``SQLiteSessionRuntimeRepository`` is a DB wrapper (refusals + store holder + one
 * transaction per body) and is not ported.  The four pure shape/identity helpers the
 * verbs go through ARE ported, including the contract package's id derivations
 * (SHA-256 of the compact JSON identity) they call.
 *
 * PoP: w19_runtime_3ded45_derive @ persistence/sqlite/authority/runtime.py:_derive
 * PoP: w19_runtime_3ded45_mutation_call @ persistence/sqlite/authority/runtime.py:_mutation_call
 * PoP: w19_runtime_3ded45_legacy_prepared @ persistence/sqlite/authority/runtime.py:_legacy_prepared
 * PoP: w19_runtime_3ded45_chat_params @ persistence/sqlite/authority/runtime.py:_chat_params
 */
#include "hermes_json.h"
#include "noc_w19_runtime_3ded45.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* -- SHA-256 (FIPS 180-4), used by the pure id derivations ------------------- */

static const unsigned SHA_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static unsigned rotr(unsigned x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_block(unsigned h[8], const unsigned char *p) {
    unsigned w[64], a, b, c, d, e, f, g, hh, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((unsigned)p[i * 4] << 24) | ((unsigned)p[i * 4 + 1] << 16) |
               ((unsigned)p[i * 4 + 2] << 8) | (unsigned)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        unsigned s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (i = 0; i < 64; i++) {
        unsigned S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        unsigned ch = (e & f) ^ ((~e) & g);
        unsigned S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        unsigned maj = (a & b) ^ (a & c) ^ (b & c);
        t1 = hh + S1 + ch + SHA_K[i] + w[i];
        t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

static void sha256_hex(const unsigned char *msg, size_t len, char *out) {
    unsigned h[8] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u };
    static const char HEXD[] = "0123456789abcdef";
    unsigned char block[64];
    unsigned long long bits = (unsigned long long)len * 8ULL;
    size_t i, full = len / 64, rem = len % 64, padlen;
    for (i = 0; i < full; i++) sha256_block(h, msg + i * 64);
    memset(block, 0, sizeof block);
    if (rem) memcpy(block, msg + full * 64, rem);
    block[rem] = 0x80;
    padlen = (rem < 56) ? 56 : 120;
    if (rem >= 56) {
        sha256_block(h, block);
        memset(block, 0, sizeof block);
    }
    (void)padlen;
    for (i = 0; i < 8; i++) block[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
    sha256_block(h, block);
    for (i = 0; i < 8; i++) {
        out[i * 8 + 0] = HEXD[(h[i] >> 28) & 0xF];
        out[i * 8 + 1] = HEXD[(h[i] >> 24) & 0xF];
        out[i * 8 + 2] = HEXD[(h[i] >> 20) & 0xF];
        out[i * 8 + 3] = HEXD[(h[i] >> 16) & 0xF];
        out[i * 8 + 4] = HEXD[(h[i] >> 12) & 0xF];
        out[i * 8 + 5] = HEXD[(h[i] >> 8) & 0xF];
        out[i * 8 + 6] = HEXD[(h[i] >> 4) & 0xF];
        out[i * 8 + 7] = HEXD[h[i] & 0xF];
    }
    out[64] = '\0';
}

/* -- compact json.dumps for the local identity (ensure_ascii=True) ------------ */

typedef struct {
    char *buf;
    size_t len, cap;
} obuf_t;

static void ob_put(obuf_t *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 64;
        char *nb;
        while (nc < b->len + n + 1) nc *= 2;
        nb = (char *)realloc(b->buf, nc);
        if (!nb) return;
        b->buf = nb;
        b->cap = nc;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static size_t u8_next(const char *s, unsigned *cp) {
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] < 0x80) { *cp = u[0]; return 1; }
    if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x1F) << 6) | (unsigned)(u[1] & 0x3F);
        return 2;
    }
    if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x0F) << 12) | ((unsigned)(u[1] & 0x3F) << 6) | (unsigned)(u[2] & 0x3F);
        return 3;
    }
    if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x07) << 18) | ((unsigned)(u[1] & 0x3F) << 12) |
              ((unsigned)(u[2] & 0x3F) << 6) | (unsigned)(u[3] & 0x3F);
        return 4;
    }
    *cp = u[0];
    return 1;
}

static void ob_pystr(obuf_t *b, const char *s) {
    char esc[16];
    unsigned cp;
    ob_put(b, "\"", 1);
    while (s && *s) {
        size_t step = u8_next(s, &cp);
        switch (cp) {
            case '"':  ob_put(b, "\\\"", 2); break;
            case '\\': ob_put(b, "\\\\", 2); break;
            case '\n': ob_put(b, "\\n", 2); break;
            case '\r': ob_put(b, "\\r", 2); break;
            case '\t': ob_put(b, "\\t", 2); break;
            case '\b': ob_put(b, "\\b", 2); break;
            case '\f': ob_put(b, "\\f", 2); break;
            default:
                if (cp < 0x20 || (cp >= 0x80 && cp <= 0xFFFF)) {
                    snprintf(esc, sizeof esc, "\\u%04x", cp);
                    ob_put(b, esc, strlen(esc));
                } else if (cp < 0x80) {
                    ob_put(b, s, 1);
                } else {
                    unsigned v = cp - 0x10000;
                    snprintf(esc, sizeof esc, "\\u%04x\\u%04x", 0xD800 | (v >> 10), 0xDC00 | (v & 0x3FF));
                    ob_put(b, esc, strlen(esc));
                }
                break;
        }
        s += step;
    }
    ob_put(b, "\"", 1);
}

/* -- the four helpers -------------------------------------------------------- */

char *w19_runtime_3ded45_derive(int kind, const char *session_id, const json_t *identity,
                               const char *profile_id, const char *principal_id,
                               const char *base_request_id, long long generation) {
    char hex[65];
    char *seed;
    size_t cap = 1, off = 0;
    int parts = 0, i;

    if (kind == W19_RUNTIME_3DED45_API) {
        char num[32];
        if (!generation) return dupstr(session_id ? session_id : "");
        snprintf(num, sizeof num, "%lld", generation);
        cap = strlen(session_id ? session_id : "") + 1 + strlen(num) + 1;
        seed = (char *)malloc(cap);
        if (!seed) return NULL;
        off = strlen(session_id ? session_id : "");
        memcpy(seed, session_id ? session_id : "", off);
        seed[off] = '\0';
        memcpy(seed + off + 1, num, strlen(num) + 1);
        off += 1 + strlen(num);
        sha256_hex((const unsigned char *)seed, off, hex);
        free(seed);
        {
            char *out = (char *)malloc(4 + 16 + 1);
            if (!out) return NULL;
            memcpy(out, "api-", 4);
            memcpy(out + 4, hex, 16);
            out[20] = '\0';
            return out;
        }
    }

    if (kind == W19_RUNTIME_3DED45_ROOM) {
        /* "\0".join(identity) + (f"\0{generation}" if generation else "") */
        if (!identity || identity->type != JSON_ARRAY) return dupstr("room_");
        parts = (int)json_len(identity);
        for (i = 0; i < parts; i++) {
            json_t *e = json_get(identity, (size_t)i);
            cap += (e && e->type == JSON_STRING && e->str_val) ? strlen(e->str_val) : 0;
        }
        cap += (size_t)parts + 32;
        seed = (char *)malloc(cap);
        if (!seed) return NULL;
        seed[0] = '\0';
        for (i = 0; i < parts; i++) {
            json_t *e = json_get(identity, (size_t)i);
            const char *v = (e && e->type == JSON_STRING && e->str_val) ? e->str_val : "";
            size_t n = strlen(v);
            if (i > 0) seed[off++] = '\0';
            memcpy(seed + off, v, n);
            off += n;
        }
        if (generation) {
            char num[32];
            snprintf(num, sizeof num, "%lld", generation);
            seed[off++] = '\0';
            memcpy(seed + off, num, strlen(num));
            off += strlen(num);
        }
        sha256_hex((const unsigned char *)seed, off, hex);
        free(seed);
        {
            char *out = (char *)malloc(5 + 32 + 1);
            if (!out) return NULL;
            memcpy(out, "room_", 5);
            memcpy(out + 5, hex, 32);
            out[37] = '\0';
            return out;
        }
    }

    /* LocalGenerations: local_session_id(profile, principal, generation_request_id(base, n)) */
    {
        char num[32];
        char *req;
        obuf_t b;
        if (!generation) {
            req = dupstr(base_request_id ? base_request_id : "");
        } else {
            size_t blen = strlen(base_request_id ? base_request_id : "");
            snprintf(num, sizeof num, "%lld", generation);
            req = (char *)malloc(blen + 1 + strlen(num) + 1);
            if (!req) return NULL;
            memcpy(req, base_request_id ? base_request_id : "", blen);
            req[blen] = ':';
            memcpy(req + blen + 1, num, strlen(num) + 1);
        }
        if (!req) return NULL;
        memset(&b, 0, sizeof b);
        ob_put(&b, "[", 1);
        ob_pystr(&b, profile_id ? profile_id : "");
        ob_put(&b, ",", 1);
        ob_pystr(&b, principal_id ? principal_id : "");
        ob_put(&b, ",", 1);
        ob_pystr(&b, req);
        ob_put(&b, "]", 1);
        free(req);
        if (!b.buf) return NULL;
        sha256_hex((const unsigned char *)b.buf, b.len, hex);
        free(b.buf);
        {
            char *out = (char *)malloc(6 + 64 + 1);
            if (!out) return NULL;
            memcpy(out, "local-", 6);
            memcpy(out + 6, hex, 64);
            out[70] = '\0';
            return out;
        }
    }
}

json_t *w19_runtime_3ded45_mutation_call(const char *operation, const json_t *payload,
                                         int payload_is_callable) {
    json_t *out = json_object();
    json_set(out, "operation", operation ? json_string(operation) : json_null());
    json_set(out, "payload", (payload_is_callable && payload) ? json_copy(payload) : json_null());
    return out;
}

json_t *w19_runtime_3ded45_legacy_prepared(int is_none, const json_t *receipt, const char *target,
                                           long long target_revision, const json_t *values) {
    json_t *out, *snap;
    size_t i;
    if (is_none) return json_null();
    out = json_object();
    snap = json_object();
    json_set(snap, "receipt", receipt ? json_copy(receipt) : json_null());
    json_set(snap, "target", target ? json_string(target) : json_null());
    json_set(snap, "target_revision", json_number((double)target_revision));
    json_set(out, "snapshot", snap);
    if (values && values->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(values); i++) {
            const char *k = json_object_get_key_at(values, i);
            if (!k) continue;
            json_set(out, k, json_copy(json_object_get_at(values, i)));
        }
    }
    return out;
}

json_t *w19_runtime_3ded45_chat_params(const char *request_id, const char *operation,
                                       const json_t *payload, long long expected_revision,
                                       long long expected_generation) {
    json_t *out = json_object();
    json_set(out, "request_id", request_id ? json_string(request_id) : json_null());
    json_set(out, "operation", operation ? json_string(operation) : json_null());
    json_set(out, "payload", payload ? json_copy(payload) : json_object());
    json_set(out, "expected_revision", json_number((double)expected_revision));
    json_set(out, "expected_generation", json_number((double)expected_generation));
    return out;
}
