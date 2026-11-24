/*
 * file_operations_native.c - C home for upstream Hermes tools/file_operations_native.py.
 *
 * PoP: w16_fileoperat_7d3fcc_probe @ tools/file_operations_native.py:probe
 * PoP: w16_fileoperat_7d3fcc_sha256_of @ tools/file_operations_native.py:sha256_of
 *
 * Faithful local-native read path: a single regular-file lookup, BOM detection
 * and (optionally capped) body read for probe; full-file SHA-256 for sha256_of.
 * Anything that is not a readable regular file follows the Python contract
 * (missing -> (False, None) / None; unexpected error -> None = ask the shell).
 */
#include "noc_w16_fileoperat_7d3fcc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

static char *hexenc(const unsigned char *d, size_t n) {
    static const char H[] = "0123456789abcdef";
    char *o = (char *)malloc(n * 2 + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; ++i) { o[i*2] = H[d[i] >> 4]; o[i*2+1] = H[d[i] & 15]; }
    o[n*2] = '\0';
    return o;
}

/* ---- self-contained SHA-256 (avoids an external link dependency) ---- */
typedef struct { uint32_t h[8]; unsigned char buf[64]; size_t buflen; uint64_t total; } sha_ctx;
static const uint32_t K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t rr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void sha_compress(sha_ctx *c, const unsigned char *p) {
    uint32_t w[64], a,b,cc,d,e,f,g,h,t1,t2;
    for (int i=0;i<16;i++) w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for (int i=16;i<64;i++) {
        uint32_t s0=rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    a=c->h[0];b=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
    for (int i=0;i<64;i++) {
        uint32_t S1=rr(e,6)^rr(e,11)^rr(e,25);
        uint32_t ch=(e&f)^((~e)&g);
        t1=h+S1+ch+K[i]+w[i];
        uint32_t S0=rr(a,2)^rr(a,13)^rr(a,22);
        uint32_t maj=(a&b)^(a&cc)^(b&cc);
        t2=S0+maj;
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
}
static void sha_init(sha_ctx *c) {
    c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;
    c->buflen=0;c->total=0;
}
static void sha_update(sha_ctx *c, const unsigned char *d, size_t n) {
    c->total += (uint64_t)n;
    while (n) {
        size_t take = 64 - c->buflen; if (take > n) take = n;
        memcpy(c->buf + c->buflen, d, take); c->buflen += take; d += take; n -= take;
        if (c->buflen == 64) { sha_compress(c, c->buf); c->buflen = 0; }
    }
}
static void sha_final(sha_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->total * 8;
    unsigned char pad = 0x80;
    sha_update(c, &pad, 1);
    unsigned char z = 0;
    while (c->buflen != 56) sha_update(c, &z, 1);
    unsigned char len[8];
    for (int i=0;i<8;i++) len[7-i]=(unsigned char)(bits>>(8*i));
    /* append length without changing total semantics */
    for (int i=0;i<8;i++) { c->buf[c->buflen++]=len[i]; if(c->buflen==64){sha_compress(c,c->buf);c->buflen=0;} }
    for (int i=0;i<8;i++) {
        out[i*4]=(unsigned char)(c->h[i]>>24);out[i*4+1]=(unsigned char)(c->h[i]>>16);
        out[i*4+2]=(unsigned char)(c->h[i]>>8);out[i*4+3]=(unsigned char)c->h[i];
    }
}

/* ---- ported API ---- */
json_t *w16_fileoperat_7d3fcc_probe(const char *path, int want_text) {
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            json_t *r = json_array(); json_append(r, json_bool(false)); json_append(r, json_null());
            return r;
        }
        return json_null();
    }
    if (!(st.st_mode & S_IFREG)) return json_null();
    FILE *f = fopen(path, "rb");
    if (!f) return json_null();
    size_t cap = want_text ? (size_t)st.st_size : ((size_t)st.st_size < 4096u ? (size_t)st.st_size : 4096u);
    unsigned char *buf = (unsigned char *)malloc(cap ? cap : 1);
    if (!buf) { fclose(f); return json_null(); }
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    int bom = n >= 3 && buf[0] == 0xef && buf[1] == 0xbb && buf[2] == 0xbf;
    char *hex = hexenc(buf, n);
    free(buf);
    if (!hex) return json_null();
    json_t *r = json_array();
    json_append(r, json_bool(bom ? true : false));
    json_append(r, json_string(hex));
    free(hex);
    return r;
}

json_t *w16_fileoperat_7d3fcc_sha256_of(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return json_null();
    if (!(st.st_mode & S_IFREG)) return json_null();
    FILE *f = fopen(path, "rb");
    if (!f) return json_null();
    sha_ctx c; sha_init(&c);
    unsigned char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) sha_update(&c, chunk, n);
    fclose(f);
    unsigned char dig[32]; sha_final(&c, dig);
    char *hex = hexenc(dig, 32);
    if (!hex) return json_null();
    json_t *r = json_string(hex);
    free(hex);
    return r;
}
