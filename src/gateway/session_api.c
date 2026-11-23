/*
 * session_api.c — faithful C11 port of the pure identity helpers in
 *                  gateway/session_api.py.
 *
 * PoP: w7_sessionapi_b56b61_api_generation_id @ gateway/session_api.py:api_generation_id
 * PoP: w7_sessionapi_b56b61_room_session_id   @ gateway/session_api.py:room_session_id
 * PoP: w7_sessionapi_b56b61_api_storage_source @ gateway/session_api.py:api_storage_source
 *
 * SHA-256 is implemented inline so the object links with only libjson.
 */
#include "noc_w7_sessionapi_b56b61.h"
#include "hermes_json.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- self-contained SHA-256 (no external link dependency) ---- */
#define W7R_XROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static const uint32_t W7R_K[64] = {
 0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
 0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
 0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
 0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
 0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
 0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
 0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
 0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};

static void w7sha256_bytes(const unsigned char *data, size_t len, unsigned char out[32]) {
    size_t total = ((len + 9 + 63) / 64) * 64;
    unsigned char *buf = (unsigned char *)calloc(total ? total : 64, 1);
    if (!buf) { memset(out, 0, 32); return; }
    memcpy(buf, data, len);
    buf[len] = 0x80;
    unsigned long long bits = (unsigned long long)len * 8ull;
    for (int i = 0; i < 8; i++) buf[total - 1 - i] = (unsigned char)(bits >> (8 * i));
    uint32_t h[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                     0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    for (size_t off = 0; off < total; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)buf[off+4*i]<<24)|((uint32_t)buf[off+4*i+1]<<16)|
                   ((uint32_t)buf[off+4*i+2]<<8)|((uint32_t)buf[off+4*i+3]);
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = W7R_XROTR(w[i-15],7)^W7R_XROTR(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1 = W7R_XROTR(w[i-2],17)^W7R_XROTR(w[i-2],19)^(w[i-2]>>10);
            w[i] = w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = W7R_XROTR(e,6)^W7R_XROTR(e,11)^W7R_XROTR(e,25);
            uint32_t ch = (e&f)^((~e)&g);
            uint32_t t1 = hh+S1+ch+W7R_K[i]+w[i];
            uint32_t S0 = W7R_XROTR(a,2)^W7R_XROTR(a,13)^W7R_XROTR(a,22);
            uint32_t maj = (a&b)^(a&c)^(b&c);
            uint32_t t2 = S0+maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    for (int i = 0; i < 8; i++) {
        out[4*i]   = (unsigned char)(h[i]>>24); out[4*i+1] = (unsigned char)(h[i]>>16);
        out[4*i+2] = (unsigned char)(h[i]>>8);  out[4*i+3] = (unsigned char)(h[i]);
    }
    free(buf);
}

static char *w7sha256_hex(const unsigned char *data, size_t len) {
    static const char *H = "0123456789abcdef";
    unsigned char dig[32];
    w7sha256_bytes(data, len, dig);
    char *out = (char *)malloc(65);
    for (int i = 0; i < 32; i++) { out[2*i]=H[dig[i]>>4]; out[2*i+1]=H[dig[i]&15]; }
    out[64] = '\0';
    return out;
}


struct json_t *w7_sessionapi_b56b61_api_generation_id(const char *session_id, int generation) {
    if (!session_id) session_id = "";
    if (!generation) return json_string(session_id); /* if not generation: return session_id */
    char gs[32];
    snprintf(gs, sizeof(gs), "%d", generation);
    size_t n = strlen(session_id);
    unsigned char *buf = (unsigned char *)malloc(n + 1 + strlen(gs) + 1);
    memcpy(buf, session_id, n);
    buf[n] = '\0';                       /* f'{session_id}\0{generation}' */
    memcpy(buf + n + 1, gs, strlen(gs));
    char *hex = w7sha256_hex(buf, n + 1 + strlen(gs));
    free(buf);
    char out[64];
    snprintf(out, sizeof(out), "api-%.16s", hex);
    free(hex);
    return json_string(out);
}

struct json_t *w7_sessionapi_b56b61_room_session_id(const struct json_t *identity, int generation) {
    /* seed = '\0'.join(identity) + (f'\0{generation}' if generation else '') */
    size_t cap = 64, len = 0;
    unsigned char *buf = (unsigned char *)malloc(cap);
    size_t count = (identity && identity->type == JSON_ARRAY) ? identity->c.count : 0;
    for (size_t i = 0; i < count; i++) {
        const char *s = (identity->c.items[i] && identity->c.items[i]->type == JSON_STRING)
                        ? identity->c.items[i]->str_val : "";
        size_t sl = strlen(s);
        if (len + sl + 1 > cap) { cap = (len + sl + 1) * 2; buf = (unsigned char *)realloc(buf, cap); }
        memcpy(buf + len, s, sl); len += sl;
        if (i + 1 < count) buf[len++] = '\0';  /* join separator */
    }
    char gs[32];
    if (generation) {
        snprintf(gs, sizeof(gs), "%d", generation);
        size_t gl = strlen(gs);
        if (len + 1 + gl > cap) { cap = (len + 1 + gl) * 2; buf = (unsigned char *)realloc(buf, cap); }
        buf[len++] = '\0';
        memcpy(buf + len, gs, gl); len += gl;
    }
    char *hex = w7sha256_hex(buf, len);
    free(buf);
    char out[64];
    snprintf(out, sizeof(out), "room_%.32s", hex);
    free(hex);
    return json_string(out);
}

struct json_t *w7_sessionapi_b56b61_api_storage_source(const struct json_t *store,
        const char *session_id, const char *fallback) {
    const char *fb = fallback ? fallback : "";
    if (strcmp(fb, "api_server")) return json_string(fb);
    struct json_t *binding = store ? json_obj_get(store, "binding") : NULL;
    if (!binding || binding->type != JSON_STRING) return json_string(fb);
    return json_copy(binding);
}
