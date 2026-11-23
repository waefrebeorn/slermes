/*
 * account_usage_cache.c - faithful C11 port of pure helpers from upstream
 * Hermes agent/account_usage_cache.py (gate w11_accountusa_57bd53).
 *
 * PoP: w11_accountusa_57bd53_key @ agent/account_usage_cache.py:_key
 * PoP: w11_accountusa_57bd53_identity_id @ agent/account_usage_cache.py:_identity_id_for
 * PoP: w11_accountusa_57bd53_snapshot_is_stale @ agent/account_usage_cache.py:snapshot_is_stale
 *
 * The thread/refresh state machine (locks, inflight set, background threads)
 * is process-bound and intentionally not ported.
 */
#include "noc_w11_accountusa_57bd53.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *str_dup_n(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static char *str_dup(const char *s) { return str_dup_n(s ? s : "", strlen(s ? s : "")); }

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
/* Python str.strip() */
static char *py_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)p[a])) a++;
    while (b > a && py_isspace((unsigned char)p[b - 1])) b--;
    return str_dup_n(p + a, b - a);
}
/* Python str.strip().lower() */
static char *py_strip_lower(const char *s) {
    char *t = py_strip(s);
    if (t) for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
    return t;
}
/* Python str.strip().rstrip("/") */
static char *py_strip_rstrip_slash(const char *s) {
    char *t = py_strip(s);
    if (!t) return NULL;
    size_t n = strlen(t);
    while (n > 0 && t[n - 1] == '/') t[--n] = '\0';
    return t;
}

json_t *w11_accountusa_57bd53_key(const char *home_key, const char *provider,
                                   const char *identity_id, const char *base_url) {
    json_t *out = json_array();
    if (!out) return NULL;
    char *hk = str_dup(home_key), *p = py_strip_lower(provider),
         *iid = py_strip(identity_id), *bu = py_strip_rstrip_slash(base_url);
    json_append(out, json_string(hk ? hk : ""));
    json_append(out, json_string(p ? p : ""));
    json_append(out, json_string(iid ? iid : ""));
    json_append(out, json_string(bu ? bu : ""));
    free(hk); free(p); free(iid); free(bu);
    return out;
}

/* ---- SHA-256 (FIPS 180-4) -------------------------------------------- */
typedef struct { uint32_t h[8]; uint64_t bitlen; unsigned char buf[64]; size_t buflen; } sha256_ctx;
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
static uint32_t rotr32(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
static void sha_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0=rotr32(w[i-15],7)^rotr32(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=rotr32(w[i-2],17)^rotr32(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1=rotr32(e,6)^rotr32(e,11)^rotr32(e,25), ch=(e&f)^((~e)&g);
        uint32_t t1=hh+S1+ch+K256[i]+w[i];
        uint32_t S0=rotr32(a,2)^rotr32(a,13)^rotr32(a,22), maj=(a&b)^(a&cc)^(b&cc);
        uint32_t t2=S0+maj; hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=hh;
}
static void sha_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667u;c->h[1]=0xbb67ae85u;c->h[2]=0x3c6ef372u;c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu;c->h[5]=0x9b05688cu;c->h[6]=0x1f83d9abu;c->h[7]=0x5be0cd19u;
    c->bitlen=0;c->buflen=0;
}
static void sha_update(sha256_ctx *c, const unsigned char *data, size_t len) {
    c->bitlen += (uint64_t)len * 8;
    while (len) {
        size_t n = 64 - c->buflen; if (n > len) n = len;
        memcpy(c->buf + c->buflen, data, n); c->buflen += n; data += n; len -= n;
        if (c->buflen == 64) { sha_block(c, c->buf); c->buflen = 0; }
    }
}
static void sha_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits=c->bitlen; unsigned char pad=0x80; sha_update(c,&pad,1);
    unsigned char zero=0; while (c->buflen!=56) sha_update(c,&zero,1);
    unsigned char tail[8]; for (int i=0;i<8;i++) tail[i]=(unsigned char)(bits>>(56-8*i));
    sha_update(c,tail,8);
    for (int i=0;i<8;i++){out[i*4]=(unsigned char)(c->h[i]>>24);out[i*4+1]=(unsigned char)(c->h[i]>>16);
        out[i*4+2]=(unsigned char)(c->h[i]>>8);out[i*4+3]=(unsigned char)(c->h[i]);}
}

char *w11_accountusa_57bd53_identity_id(const char *provider, const char *runtime_api_key,
                                         const char *entry_id, const json_t *principal) {
    char *token = py_strip(runtime_api_key);
    if (!token || !*token) {
        free(token);
        size_t n = strlen("entry:") + strlen(entry_id ? entry_id : "") + 1;
        char *out = (char *)malloc(n);
        if (out) snprintf(out, n, "entry:%s", entry_id ? entry_id : "");
        return out;
    }
    if (provider && strcmp(provider, "openai-codex") == 0 && principal &&
        principal->type == JSON_ARRAY && principal->c.count >= 2) {
        json_t *a0 = json_get((json_t *)principal, 0), *a1 = json_get((json_t *)principal, 1);
        const char *s0 = (a0 && a0->type == JSON_STRING) ? (a0->str_val ? a0->str_val : "") : "";
        const char *s1 = (a1 && a1->type == JSON_STRING) ? (a1->str_val ? a1->str_val : "") : "";
        size_t n = strlen("codex:") + strlen(s0) + 1 + strlen(s1) + 1;
        char *out = (char *)malloc(n);
        if (out) snprintf(out, n, "codex:%s:%s", s0, s1);
        free(token);
        return out;
    }
    sha256_ctx ctx; sha_init(&ctx);
    sha_update(&ctx, (const unsigned char *)token, strlen(token));
    unsigned char d[32]; sha_final(&ctx, d);
    static const char hex[] = "0123456789abcdef";
    char hx[17];
    for (int i = 0; i < 8; i++) { hx[i*2] = hex[d[i] >> 4]; hx[i*2+1] = hex[d[i] & 0x0f]; }
    hx[16] = '\0';
    free(token);
    size_t n = strlen("fp:") + strlen(hx) + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "fp:%s", hx);
    return out;
}

bool w11_accountusa_57bd53_snapshot_is_stale(bool valid, bool bad_timestamp,
                                              double fetched_at, double now) {
    if (!valid || bad_timestamp) return true;
    double age = now - fetched_at;
    return age < 0 || age > 900.0;
}
