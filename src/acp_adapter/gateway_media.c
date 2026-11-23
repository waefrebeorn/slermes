/*
 * gateway_media.c - faithful C11 port of submit_fingerprint from upstream
 * Hermes acp_adapter/gateway_media.py (gate w11_gatewaymed_35cebd).
 *
 * PoP: w11_gatewaymed_35cebd_submit_fingerprint @ acp_adapter/gateway_media.py:submit_fingerprint
 *
 * The async staging helpers (stage_user_content/_fetch_remote_image) require
 * network, image sniffing and a profile cache, so they are not ported here.
 */
#include "noc_w11_gatewaymed_35cebd.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

static void sha256_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + K256[i] + w[i];
        uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
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

static void sha256_update(sha256_ctx *c, const unsigned char *data, size_t len) {
    c->bitlen += (uint64_t)len * 8;
    while (len) {
        size_t n = 64 - c->buflen;
        if (n > len) n = len;
        memcpy(c->buf + c->buflen, data, n);
        c->buflen += n;
        data += n; len -= n;
        if (c->buflen == 64) { sha256_block(c, c->buf); c->buflen = 0; }
    }
}

static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bitlen;
    unsigned char pad = 0x80;
    sha256_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->buflen != 56) sha256_update(c, &zero, 1);
    unsigned char tail[8];
    for (int i = 0; i < 8; i++) tail[i] = (unsigned char)(bits >> (56 - 8*i));
    sha256_update(c, tail, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)(c->h[i]);
    }
}

/* Python len(str) = number of Unicode code points (UTF-8 leading bytes). */
static size_t py_str_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

static void feed_part(sha256_ctx *ctx, const char *kind, const char *value) {
    sha256_update(ctx, (const unsigned char *)kind, strlen(kind));
    size_t n = py_str_len(value);
    unsigned char be[8];
    for (int i = 0; i < 8; i++) be[i] = (unsigned char)(((uint64_t)n) >> (56 - 8*i));
    sha256_update(ctx, be, 8);
    sha256_update(ctx, (const unsigned char *)value, strlen(value));
}

json_t *w11_gatewaymed_35cebd_submit_fingerprint(const char *session_id, const json_t *content) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    const char *sid = session_id ? session_id : "";

    if (content && content->type == JSON_STRING) {
        feed_part(&ctx, "text", content->str_val ? content->str_val : "");
    } else if (content && content->type == JSON_ARRAY) {
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *part = content->c.items[i];
            if (part && part->type == JSON_STRING) {
                feed_part(&ctx, "text", part->str_val ? part->str_val : "");
            } else if (part && part->type == JSON_OBJECT) {
                const char *ptype = json_get_str(part, "type", "");
                if (ptype && !strcmp(ptype, "text")) {
                    feed_part(&ctx, "text", json_get_str(part, "text", ""));
                } else {
                    json_t *iu = json_obj_get(part, "image_url");
                    const char *url = (iu && iu->type == JSON_OBJECT)
                                        ? json_get_str(iu, "url", "") : "";
                    feed_part(&ctx, "image", url);
                }
            }
        }
    }

    unsigned char digest[32];
    sha256_final(&ctx, digest);
    static const char hex[] = "0123456789abcdef";
    char hx[65];
    for (int i = 0; i < 32; i++) {
        hx[i*2]   = hex[digest[i] >> 4];
        hx[i*2+1] = hex[digest[i] & 0x0f];
    }
    hx[64] = '\0';

    json_t *out = json_array();
    if (!out) return NULL;
    json_append(out, json_string(sid));
    json_append(out, json_string(hx));
    return out;
}
