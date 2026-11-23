/*
 * session_persistence.c — C home for upstream Hermes agent/session_persistence.py.
 *
 * Faithful C11 ports of the pure session-persistence helpers: the persist
 * override admission test, the merged-turn-prefix content override, and the
 * path-safe session filename component (including the SHA-256 collision
 * suffix when the sanitized id differs from the raw id).
 *
 * PoP: w11_sessionper_908f54_override @ agent/session_persistence.py:_override_replaces_content
 * PoP: w11_sessionper_908f54_content_override @ agent/session_persistence.py:_content_with_turn_override
 * PoP: w11_sessionper_908f54_safe_filename @ agent/session_persistence.py:_safe_session_filename_component
 */
#include "noc_w11_sessionper_908f54.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPRESSED_SUMMARY_KEY "_compressed_summary"
#define MERGED_TURN_PREFIX_KEY "_merged_turn_prefix"

static int py_truthy(json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* str.strip() over the ASCII whitespace class used by Python's default strip. */
static char *py_strip(char *s) {
    if (!s) return NULL;
    static const char *ws = " \t\n\r\f\v";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(ws, s[a])) a++;
    while (b > a && strchr(ws, s[b - 1])) b--;
    char *r = (char *)malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    free(s);
    return r;
}

/* ---- Minimal self-contained SHA-256 (FIPS 180-4) ---- */
typedef struct { uint32_t h[8]; uint64_t bitlen; unsigned char buf[64]; size_t buflen; } sha256_ctx;

static const uint32_t SHA_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
};
static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

static void sha_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (size_t i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4*i] << 24) | ((uint32_t)p[4*i+1] << 16) |
               ((uint32_t)p[4*i+2] << 8) | (uint32_t)p[4*i+3];
    for (size_t i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (size_t i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + SHA_K[i] + w[i];
        uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}
static void sha_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen=0; c->buflen=0;
}
static void sha_update(sha256_ctx *c, const unsigned char *p, size_t n) {
    c->bitlen += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take; p += take; n -= take;
        if (c->buflen == 64) { sha_block(c, c->buf); c->buflen = 0; }
    }
}
static void sha_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bitlen;
    unsigned char pad = 0x80;
    sha_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->buflen != 56) sha_update(c, &zero, 1);
    unsigned char len[8];
    for (size_t i = 0; i < 8; i++) len[i] = (unsigned char)(bits >> (56 - 8*i));
    /* append length without disturbing bitlen accounting */
    for (size_t i = 0; i < 8; i++) {
        c->buf[c->buflen++] = len[i];
        if (c->buflen == 64) { sha_block(c, c->buf); c->buflen = 0; }
    }
    for (size_t i = 0; i < 8; i++)
        for (size_t j = 0; j < 4; j++)
            out[4*i+j] = (unsigned char)(c->h[i] >> (24 - 8*j));
}

/* PoP: w11_sessionper_908f54_override @ agent/session_persistence.py:_override_replaces_content */
json_t *w11_sessionper_908f54_override(json_t *a) {
    json_t *msg = a ? json_obj_get(a, "msg") : NULL;
    json_t *content = a ? json_obj_get(a, "content") : NULL;
    json_t *override = a ? json_obj_get(a, "override") : NULL;
    int ok = override && !json_is_null(override)
             && !(msg && json_is_object(msg) && py_truthy(json_obj_get(msg, COMPRESSED_SUMMARY_KEY)))
             && !(json_is_array(content) && !json_is_array(override));
    return json_bool(ok ? true : false);
}

/* PoP: w11_sessionper_908f54_content_override @ agent/session_persistence.py:_content_with_turn_override */
json_t *w11_sessionper_908f54_content_override(json_t *a) {
    json_t *msg = a ? json_obj_get(a, "msg") : NULL;
    json_t *content = a ? json_obj_get(a, "content") : NULL;
    json_t *override = a ? json_obj_get(a, "override") : NULL;
    json_t *prefix = (msg && json_is_object(msg)) ? json_obj_get(msg, MERGED_TURN_PREFIX_KEY) : NULL;
    if (json_is_string(content) && json_is_string(override) && json_is_string(prefix)
        && content->str_val && override->str_val && prefix->str_val) {
        const char *c = content->str_val, *p = prefix->str_val;
        size_t lp = strlen(p);
        int match = !strcmp(c, p) || (!strncmp(c, p, lp) && !strncmp(c + lp, "\n\n", 2));
        if (match) {
            if (override->str_val[0]) {
                size_t n = lp + 2 + strlen(override->str_val);
                char *r = (char *)malloc(n + 1);
                if (r) {
                    memcpy(r, p, lp); memcpy(r + lp, "\n\n", 2);
                    strcpy(r + lp + 2, override->str_val);
                    json_t *out = json_string(r); free(r); return out;
                }
            }
            return json_copy(prefix);
        }
    }
    return override ? json_copy(override) : json_null();
}

/* PoP: w11_sessionper_908f54_safe_filename @ agent/session_persistence.py:_safe_session_filename_component */
json_t *w11_sessionper_908f54_safe_filename(json_t *a) {
    json_t *sv = a ? json_obj_get(a, "session_id") : NULL;
    char *raw;
    if (!py_truthy(sv)) {
        raw = (char *)strdup("");
    } else if (json_is_string(sv)) {
        raw = (char *)strdup(sv->str_val ? sv->str_val : "");
    } else if (json_is_number(sv) && sv->num_val == (double)(long long)sv->num_val) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%lld", (long long)sv->num_val);
        raw = (char *)strdup(buf);
    } else if (json_is_bool(sv)) {
        raw = (char *)strdup(sv->bool_val ? "True" : "False");
    } else {
        raw = (char *)strdup("");
    }
    if (!raw) return json_null();
    raw = py_strip(raw);
    if (!raw) return json_null();

    size_t n = strlen(raw);
    char *san = (char *)malloc(n + 1);
    if (!san) { free(raw); return json_null(); }
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)raw[i];
        san[i] = ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '_' || ch == '-') ? (char)ch : '_';
    }
    san[n] = '\0';
    /* .strip("._") */
    size_t s = 0, e = n;
    while (s < e && (san[s] == '.' || san[s] == '_')) s++;
    while (e > s && (san[e-1] == '.' || san[e-1] == '_')) e--;
    memmove(san, san + s, e - s);
    san[e - s] = '\0';
    if (strlen(san) > 96) san[96] = '\0';
    if (!san[0]) { free(san); san = (char *)strdup("session"); }

    json_t *out;
    if (raw[0] && !strcmp(san, raw)) {
        out = json_string(san);
    } else {
        unsigned char dig[32];
        sha256_ctx ctx; sha_init(&ctx);
        sha_update(&ctx, (const unsigned char *)raw, strlen(raw));
        sha_final(&ctx, dig);
        static const char hex[] = "0123456789abcdef";
        char suffix[13];
        for (size_t i = 0; i < 6; i++) { suffix[2*i] = hex[dig[i] >> 4]; suffix[2*i+1] = hex[dig[i] & 15]; }
        suffix[12] = '\0';
        size_t len = strlen(san) + 1 + 12;
        char *full = (char *)malloc(len + 1);
        if (full) {
            snprintf(full, len + 1, "%s_%s", san, suffix);
            out = json_string(full);
            free(full);
        } else out = json_null();
    }
    free(san);
    free(raw);
    return out ? out : json_null();
}
