/*
 * auth.c - C11 port of upstream gateway/relay/auth.py (gate w24_auth_6661f1).
 *
 * PoP: w24_auth_6661f1_sign @ gateway/relay/auth.py:sign
 * PoP: w24_auth_6661f1_token @ gateway/relay/auth.py:make_token
 * PoP: w24_auth_6661f1_upgrade_token @ gateway/relay/auth.py:make_upgrade_token
 * PoP: w24_auth_6661f1_constants @ gateway/relay/auth.py:_DEFAULT_UPGRADE_TTL_SECONDS
 *
 * What is ported is the whole wire format: HMAC-SHA256 over UTF-8 as a hex
 * digest, the f"{payload}:{exp}:{sig}" frame, and Node's unpadded base64url.
 * The clock is an input (``now``) rather than a seam, so the bytes the
 * connector's relayAuthToken.ts expects are reproducible here.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_auth_6661f1.h"

/* ---- SHA-256 (FIPS 180-4) --------------------------------------------- */
typedef struct {
    uint32_t h[8];
    uint64_t bitlen;
    unsigned char buf[64];
    size_t buflen;
} sha256_ctx;

static const uint32_t K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
};

static uint32_t auth_rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

static void auth_sha_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = auth_rotr(w[i-15],7) ^ auth_rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = auth_rotr(w[i-2],17) ^ auth_rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    {
        uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
        for (i = 0; i < 64; i++) {
            uint32_t S1 = auth_rotr(e,6) ^ auth_rotr(e,11) ^ auth_rotr(e,25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = h + S1 + ch + K256[i] + w[i];
            uint32_t S0 = auth_rotr(a,2) ^ auth_rotr(a,13) ^ auth_rotr(a,22);
            uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
            uint32_t t2 = S0 + maj;
            h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
        }
        c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
        c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
    }
}

static void auth_sha_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen=0; c->buflen=0;
}

static void auth_sha_update(sha256_ctx *c, const unsigned char *data, size_t len) {
    c->bitlen += (uint64_t)len * 8;
    while (len) {
        size_t n = 64 - c->buflen;
        if (n > len) n = len;
        memcpy(c->buf + c->buflen, data, n);
        c->buflen += n;
        data += n; len -= n;
        if (c->buflen == 64) { auth_sha_block(c, c->buf); c->buflen = 0; }
    }
}

static void auth_sha_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bitlen;
    unsigned char pad = 0x80, zero = 0, tail[8];
    int i;
    auth_sha_update(c, &pad, 1);
    while (c->buflen != 56) auth_sha_update(c, &zero, 1);
    for (i = 0; i < 8; i++) tail[i] = (unsigned char)(bits >> (56 - 8*i));
    auth_sha_update(c, tail, 8);
    for (i = 0; i < 8; i++) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)(c->h[i]);
    }
}

/* ---- HMAC-SHA256 ------------------------------------------------------- */
static void auth_hmac(const char *payload, const char *secret, unsigned char out[32]) {
    unsigned char key[64], ipad[64], opad[64], inner[32];
    sha256_ctx c;
    size_t klen = secret ? strlen(secret) : 0, plen = payload ? strlen(payload) : 0;
    size_t i;

    memset(key, 0, sizeof key);
    if (klen > 64) {
        sha256_ctx kc;
        auth_sha_init(&kc);
        auth_sha_update(&kc, (const unsigned char *)secret, klen);
        auth_sha_final(&kc, key);
    } else if (klen) {
        memcpy(key, secret, klen);
    }
    for (i = 0; i < 64; i++) { ipad[i] = (unsigned char)(key[i] ^ 0x36); opad[i] = (unsigned char)(key[i] ^ 0x5c); }

    auth_sha_init(&c);
    auth_sha_update(&c, ipad, 64);
    if (plen) auth_sha_update(&c, (const unsigned char *)payload, plen);
    auth_sha_final(&c, inner);

    auth_sha_init(&c);
    auth_sha_update(&c, opad, 64);
    auth_sha_update(&c, inner, 32);
    auth_sha_final(&c, out);
}

char *w24_auth_6661f1_sign_hex(const char *payload, const char *secret) {
    static const char hexd[] = "0123456789abcdef";
    unsigned char mac[32];
    char *out = (char *)malloc(65);
    size_t i;
    if (!out) return NULL;
    auth_hmac(payload, secret, mac);
    for (i = 0; i < 32; i++) {
        out[i*2]   = hexd[mac[i] >> 4];
        out[i*2+1] = hexd[mac[i] & 0x0f];
    }
    out[64] = '\0';
    return out;
}

/* ---- unpadded base64url (Node Buffer.toString("base64url")) ------------ */
char *w24_auth_6661f1_b64url(const unsigned char *data, size_t len) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t i, o = 0, need = ((len + 2) / 3) * 4 + 1;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    for (i = 0; i + 2 < len; i += 3) {
        unsigned v = ((unsigned)data[i] << 16) | ((unsigned)data[i+1] << 8) | data[i+2];
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        out[o++] = tbl[(v >> 6) & 63];
        out[o++] = tbl[v & 63];
    }
    if (i < len) {
        unsigned v = (unsigned)data[i] << 16;
        int rest = (int)(len - i);
        if (rest == 2) v |= (unsigned)data[i+1] << 8;
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        if (rest == 2) out[o++] = tbl[(v >> 6) & 63];
    }
    out[o] = '\0';
    return out;
}

json_t *w24_auth_6661f1_make_token(const char *payload, const char *secret,
                                   long long ttl_seconds, long long now) {
    long long exp = ttl_seconds > 0 ? now + ttl_seconds : 0;
    char *frame, *sig, *raw, *enc;
    size_t need;
    json_t *out;

    if (!payload) payload = "";
    if (!secret) secret = "";
    need = strlen(payload) + 40;
    frame = (char *)malloc(need);
    if (!frame) return json_null();
    snprintf(frame, need, "%s:%lld", payload, exp);
    sig = w24_auth_6661f1_sign_hex(frame, secret);
    if (!sig) { free(frame); return json_null(); }

    need = strlen(frame) + strlen(sig) + 2;
    raw = (char *)malloc(need);
    if (!raw) { free(sig); free(frame); return json_null(); }
    snprintf(raw, need, "%s:%s", frame, sig);

    enc = w24_auth_6661f1_b64url((const unsigned char *)raw, strlen(raw));
    free(raw);
    free(sig);
    free(frame);
    if (!enc) return json_null();
    out = json_string(enc);
    free(enc);
    return out;
}

/* ---- JSON wrappers ----------------------------------------------------- */
static int auth_str_arg(const json_t *args, const char *key, const char **out) {
    const json_t *v = json_obj_get(args, key);
    if (!v || v->type != JSON_STRING) return 0;
    *out = v->str_val ? v->str_val : "";
    return 1;
}

/* int(value) for the shapes json can carry.  *present tells an absent key
 * (Python's default) apart from an explicit null (Python raises TypeError). */
static int auth_int_arg(const json_t *args, const char *key, long long *out, int *present) {
    const json_t *v = json_obj_get(args, key);
    *present = v != NULL;
    if (!v) { *out = 0; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    return 0;
}

/* int(ttl_seconds): absent -> ``def`` (the Python default argument), a number
 * or bool -> int(value), anything else (null/str/list) -> the Python TypeError
 * seam, reported as failure so the wrapper yields null. */
static int auth_ttl_arg(const json_t *args, long long *out, long long def) {
    const json_t *v = json_obj_get(args, "ttl_seconds");
    *out = def;
    if (!v) return 1;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    return 0;
}

json_t *w24_auth_6661f1_sign(const json_t *args) {
    const char *payload, *secret;
    char *hex;
    json_t *out;
    if (!auth_str_arg(args, "payload", &payload)) return json_null();
    if (!auth_str_arg(args, "secret", &secret)) return json_null();
    hex = w24_auth_6661f1_sign_hex(payload, secret);
    if (!hex) return json_null();
    out = json_string(hex);
    free(hex);
    return out;
}

json_t *w24_auth_6661f1_token(const json_t *args) {
    const char *payload, *secret;
    long long ttl = 0, now = 0;
    int present = 0;
    if (!auth_str_arg(args, "payload", &payload)) return json_null();
    if (!auth_str_arg(args, "secret", &secret)) return json_null();
    if (!auth_ttl_arg(args, &ttl, 0)) return json_null();
    if (!auth_int_arg(args, "now", &now, &present)) return json_null();
    return w24_auth_6661f1_make_token(payload, secret, ttl, now);
}

json_t *w24_auth_6661f1_upgrade_token(const json_t *args) {
    const char *gateway_id, *secret;
    long long ttl = W24_AUTH_6661F1_DEFAULT_UPGRADE_TTL, now = 0;
    int present = 0;
    if (!auth_str_arg(args, "gateway_id", &gateway_id)) return json_null();
    if (!auth_str_arg(args, "secret", &secret)) return json_null();
    if (!auth_ttl_arg(args, &ttl, W24_AUTH_6661F1_DEFAULT_UPGRADE_TTL)) return json_null();
    if (!auth_int_arg(args, "now", &now, &present)) return json_null();
    return w24_auth_6661f1_make_token(gateway_id, secret, ttl, now);
}

json_t *w24_auth_6661f1_constants(void) {
    json_t *o = json_object();
    json_set(o, "default_upgrade_ttl_seconds",
             json_number((double)W24_AUTH_6661F1_DEFAULT_UPGRADE_TTL));
    return o;
}
