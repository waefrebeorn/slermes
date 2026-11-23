/*
 * session_local_mcp.c — C home for upstream Hermes gateway/session_local_mcp.py.
 *
 * PoP: w12_sessionloc_cc8104_session_scope @ gateway/session_local_mcp.py:session_scope
 * PoP: w12_sessionloc_cc8104_frozen_toolset @ gateway/session_local_mcp.py:_frozen_toolset
 */
#include "noc_w12_sessionloc_cc8104.h"

#include "hermes_json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Minimal, self-contained SHA-256 (FIPS 180-4) ---- */
typedef struct {
    uint32_t h[8];
    uint64_t total;
    unsigned char buf[64];
    size_t used;
} sha256_ctx;

static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_compress(sha256_ctx *c, const unsigned char block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | (uint32_t)block[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i-15],7) ^ rotr32(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i-2],17) ^ rotr32(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0], b=c->h[1], cc=c->h[2], d=c->h[3];
    uint32_t e=c->h[4], f=c->h[5], g=c->h[6], hh=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + SHA256_K[i] + w[i];
        uint32_t S0 = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}

static void sha256_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667; c->h[1]=0xbb67ae85; c->h[2]=0x3c6ef372; c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f; c->h[5]=0x9b05688c; c->h[6]=0x1f83d9ab; c->h[7]=0x5be0cd19;
    c->total = 0; c->used = 0;
}

static void sha256_update(sha256_ctx *c, const unsigned char *p, size_t n) {
    c->total += n;
    while (n) {
        size_t take = 64 - c->used;
        if (take > n) take = n;
        memcpy(c->buf + c->used, p, take);
        c->used += take; p += take; n -= take;
        if (c->used == 64) { sha256_compress(c, c->buf); c->used = 0; }
    }
}

static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->total * 8;
    unsigned char one = 0x80, zero = 0x00;
    sha256_update(c, &one, 1);
    while (c->used != 56) sha256_update(c, &zero, 1);
    unsigned char lenb[8];
    for (int i = 0; i < 8; ++i) lenb[i] = (unsigned char)(bits >> (56 - 8*i));
    sha256_update(c, lenb, 8);
    for (int i = 0; i < 8; ++i) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)(c->h[i]);
    }
}

/* hexdigest of a NUL-terminated UTF-8 string; out must have >= 65 bytes. */
static void sha256_hexstr(const char *s, char out[65]) {
    static const char HEX[] = "0123456789abcdef";
    sha256_ctx c; sha256_init(&c);
    sha256_update(&c, (const unsigned char *)s, strlen(s));
    unsigned char d[32]; sha256_final(&c, d);
    for (int i = 0; i < 32; ++i) {
        out[i*2]   = HEX[d[i] >> 4];
        out[i*2+1] = HEX[d[i] & 15];
    }
    out[64] = '\0';
}

static int py_falsy(const json_t *v) {
    if (!v) return 1;
    switch (v->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default:          return 1;
    }
}

char *w12_sessionloc_cc8104_session_scope(const char *profile_id, const char *sid) {
    const char *p = profile_id ? profile_id : "";
    const char *s = sid ? sid : "";
    size_t n = strlen(p) + 1 + strlen(s) + 1;
    char *key = (char *)malloc(n);
    if (!key) return NULL;
    snprintf(key, n, "%s:%s", p, s);
    char digest[65];
    sha256_hexstr(key, digest);
    free(key);
    const char *PREFIX = "editor-session:";
    size_t out_len = strlen(PREFIX) + 64 + 1;
    char *out = (char *)malloc(out_len);
    if (!out) return NULL;
    snprintf(out, out_len, "%s%s", PREFIX, digest);
    return out;
}

int w12_sessionloc_cc8104_frozen_toolset(const struct json_t *data, const char *toolset) {
    const char *ts = toolset ? toolset : "";
    if (!data || py_falsy(data)) return 0;
    if (data->type != JSON_OBJECT) return 0;
    if (json_has(data, "names")) {
        json_t *names = json_obj_get(data, "names");
        if (!names || names->type != JSON_OBJECT) return 0;
        for (size_t i = 0; i < json_object_size(names); ++i) {
            json_t *v = json_object_get_at(names, i);
            if (!v || v->type != JSON_STRING || !v->str_val) continue;
            size_t need = 4 + strlen(v->str_val) + 1;
            char *candidate = (char *)malloc(need);
            if (!candidate) continue;
            snprintf(candidate, need, "mcp-%s", v->str_val);
            int hit = !strcmp(candidate, ts);
            free(candidate);
            if (hit) return 1;
        }
        return 0;
    }
    json_t *scope = json_obj_get(data, "scope");
    if (!scope || scope->type != JSON_STRING || !scope->str_val) return 0;
    char digest[65];
    sha256_hexstr(scope->str_val, digest);
    char prefix[65];
    snprintf(prefix, sizeof(prefix), "mcp-editor_%.20s_", digest);
    return strncmp(ts, prefix, strlen(prefix)) == 0 ? 1 : 0;
}
