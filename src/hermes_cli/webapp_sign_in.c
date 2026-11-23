/*
 * webapp_sign_in.c — C home for upstream Hermes hermes_cli/webapp_sign_in.py.
 *
 * PoP: w12_webappsign_185f5a_binding @ hermes_cli/webapp_sign_in.py:binding
 * PoP: w12_webappsign_185f5a_digest  @ hermes_cli/webapp_sign_in.py:_digest
 * PoP: w12_webappsign_185f5a_applies @ hermes_cli/webapp_sign_in.py:applies
 *
 * Self-contained SHA-256 / HMAC-SHA256 (no external crypto link) mirroring
 * hashlib/hmac exactly: binding() = hex(HMAC(token, b"webapp-sign-in")),
 * digest() = hex(SHA256(token)).
 */
#include "noc_w12_webappsign_185f5a.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- minimal SHA-256 ---------- */
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
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha256_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + K256[i] + w[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}
static void sha256_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen=0; c->buflen=0;
}
static void sha256_update(sha256_ctx *c, const unsigned char *data, size_t n) {
    c->bitlen += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->buflen; if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == 64) { sha256_block(c, c->buf); c->buflen = 0; }
    }
}
static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bitlen;
    unsigned char pad = 0x80;
    sha256_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->buflen != 56) sha256_update(c, &zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8*i));
    sha256_update(c, len, 8);
    for (int i = 0; i < 8; ++i) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)c->h[i];
    }
}
static void sha256_raw(const unsigned char *data, size_t n, unsigned char out[32]) {
    sha256_ctx c; sha256_init(&c); sha256_update(&c, data, n); sha256_final(&c, out);
}
static void hexdigest(const unsigned char dig[32], char out[65]) {
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) { out[i*2] = hx[dig[i] >> 4]; out[i*2+1] = hx[dig[i] & 15]; }
    out[64] = '\0';
}
static void sha256_hex_str(const char *s, char out[65]) {
    unsigned char dig[32];
    sha256_raw((const unsigned char *)s, strlen(s), dig);
    hexdigest(dig, out);
}
static void hmac_sha256_hex(const unsigned char *key, size_t klen,
                            const unsigned char *msg, size_t mlen, char out[65]) {
    unsigned char k[64], ipad[64], opad[64], inner[32], dig[32];
    memset(k, 0, sizeof(k));
    if (klen > 64) { sha256_raw(key, klen, k); klen = 32; }
    else memcpy(k, key, klen);
    for (int i = 0; i < 64; ++i) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }
    sha256_ctx c;
    sha256_init(&c); sha256_update(&c, ipad, 64); sha256_update(&c, msg, mlen); sha256_final(&c, inner);
    sha256_init(&c); sha256_update(&c, opad, 64); sha256_update(&c, inner, 32); sha256_final(&c, dig);
    hexdigest(dig, out);
}

char *w12_webappsign_185f5a_binding(const char *token) {
    if (!token) token = "";
    char out[65];
    hmac_sha256_hex((const unsigned char *)token, strlen(token),
                    (const unsigned char *)"webapp-sign-in", 14, out);
    return strdup(out);
}

char *w12_webappsign_185f5a_digest(const char *token) {
    if (!token) token = "";
    char out[65];
    sha256_hex_str(token, out);
    return strdup(out);
}

int w12_webappsign_185f5a_applies(const char *ui_surface, int auth_required) {
    return (ui_surface && !strcmp(ui_surface, "webapp") && auth_required) ? 1 : 0;
}
