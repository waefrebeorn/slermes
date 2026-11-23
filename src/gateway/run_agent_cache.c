/*
 * run_agent_cache.c — C home for upstream Hermes gateway/run_agent_cache.py.
 *
 * PoP: w9_runagentca_634fc9_first @ gateway/run_agent_cache.py:_first_agent
 * PoP: w9_runagentca_634fc9_tuple @ gateway/run_agent_cache.py:_tuple_agent
 * PoP: w9_runagentca_634fc9_config_sig @ gateway/run_agent_cache.py:GatewayAgentCacheMixin._agent_config_signature
 */
#include "hermes_json.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_runagentca_634fc9.h"

/* ---------------- self-contained SHA-256 (no libcrypto link dependency) ---------------- */
typedef struct {
    uint32_t h[8];
    unsigned char blk[64];
    size_t blen;
    uint64_t bits;
} sha_ctx;

static const uint32_t SHA_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
static uint32_t sha_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void sha_compress(sha_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4*i]<<24) | ((uint32_t)p[4*i+1]<<16) | ((uint32_t)p[4*i+2]<<8) | (uint32_t)p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = sha_rotr(w[i-15],7) ^ sha_rotr(w[i-15],18) ^ (w[i-15]>>3);
        uint32_t s1 = sha_rotr(w[i-2],17) ^ sha_rotr(w[i-2],19) ^ (w[i-2]>>10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = sha_rotr(e,6)^sha_rotr(e,11)^sha_rotr(e,25);
        uint32_t ch = (e&f)^((~e)&g);
        uint32_t t1 = h + S1 + ch + SHA_K[i] + w[i];
        uint32_t S0 = sha_rotr(a,2)^sha_rotr(a,13)^sha_rotr(a,22);
        uint32_t maj = (a&b)^(a&cc)^(b&cc);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}
static void sha_init(sha_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->blen=0; c->bits=0;
}
static void sha_update(sha_ctx *c, const unsigned char *data, size_t n) {
    c->bits += (uint64_t)n * 8u;
    while (n) {
        size_t take = 64 - c->blen;
        if (take > n) take = n;
        memcpy(c->blk + c->blen, data, take);
        c->blen += take; data += take; n -= take;
        if (c->blen == 64) { sha_compress(c, c->blk); c->blen = 0; }
    }
}
static void sha_final(sha_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bits;
    unsigned char pad = 0x80;
    sha_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->blen != 56) sha_update(c, &zero, 1);
    unsigned char tail[8];
    for (int i = 0; i < 8; i++) tail[i] = (unsigned char)(bits >> (56 - 8*i));
    sha_update(c, tail, 8);
    for (int i = 0; i < 8; i++) {
        out[4*i]   = (unsigned char)(c->h[i]>>24);
        out[4*i+1] = (unsigned char)(c->h[i]>>16);
        out[4*i+2] = (unsigned char)(c->h[i]>>8);
        out[4*i+3] = (unsigned char)(c->h[i]);
    }
}
static void sha256_hex(const char *data, size_t len, char out[65]) {
    sha_ctx c; unsigned char digest[32];
    sha_init(&c);
    sha_update(&c, (const unsigned char *)data, len);
    sha_final(&c, digest);
    static const char hexchars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { out[2*i]=hexchars[digest[i]>>4]; out[2*i+1]=hexchars[digest[i]&15]; }
    out[64] = '\0';
}

/* ---------------- json.dumps-compatible serialization (ASCII subset) ---------------- */
typedef struct { char buf[65536]; size_t len; } blob;
static void puts_b(blob *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > sizeof(b->buf)) n = sizeof(b->buf) - b->len - 1;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}
/* json.dumps string escaping (ensure_ascii=True); fixtures use ASCII text. */
static void put_json_string(blob *b, const char *s) {
    if (!s) { puts_b(b, "\"\""); return; }
    char tmp[16];
    puts_b(b, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '"') puts_b(b, "\\\"");
        else if (c == '\\') puts_b(b, "\\\\");
        else if (c == '\b') puts_b(b, "\\b");
        else if (c == '\f') puts_b(b, "\\f");
        else if (c == '\n') puts_b(b, "\\n");
        else if (c == '\r') puts_b(b, "\\r");
        else if (c == '\t') puts_b(b, "\\t");
        else if (c < 0x20) { snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)c); puts_b(b, tmp); }
        else { char one[2] = {(char)c, 0}; puts_b(b, one); }
    }
    puts_b(b, "\"");
}
static int cmp_strptr(const void *a, const void *b) {
    const char *const *pa = (const char *const *)a;
    const char *const *pb = (const char *const *)b;
    return strcmp(*pa, *pb);
}
/* sorted((d or {}).items()) -> [[k,v], [k,v], ...] with sort_keys-style order */
static void put_sorted_items(blob *b, const json_t *d) {
    size_t n = (d && d->type == JSON_OBJECT) ? json_object_size(d) : 0;
    puts_b(b, "[");
    if (n) {
        const char **keys = (const char **)malloc(n * sizeof(const char *));
        if (keys) {
            for (size_t i = 0; i < n; i++) keys[i] = json_object_get_key_at(d, i) ? json_object_get_key_at(d, i) : "";
            qsort(keys, n, sizeof(const char *), cmp_strptr);
            for (size_t i = 0; i < n; i++) {
                if (i) puts_b(b, ", ");
                puts_b(b, "[");
                put_json_string(b, keys[i]);
                puts_b(b, ", ");
                put_json_string(b, json_get_str(d, keys[i], ""));
                puts_b(b, "]");
            }
            free(keys);
        }
    }
    puts_b(b, "]");
}
static void put_sorted_list(blob *b, const json_t *arr) {
    size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    puts_b(b, "[");
    if (n) {
        const char **vals = (const char **)malloc(n * sizeof(const char *));
        if (vals) {
            for (size_t i = 0; i < n; i++) {
                const json_t *v = json_get(arr, i);
                vals[i] = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
            }
            qsort(vals, n, sizeof(const char *), cmp_strptr);
            for (size_t i = 0; i < n; i++) {
                if (i) puts_b(b, ", ");
                put_json_string(b, vals[i]);
            }
            free(vals);
        }
    }
    puts_b(b, "]");
}

/* ---------------- PoP exports ---------------- */
json_t *w9_runagentca_634fc9_first(const struct json_t *in) {
    const json_t *entry = json_obj_get(in, "entry");
    bool as_tuple = json_get_bool(in, "as_tuple", false);
    if (as_tuple && entry && entry->type == JSON_ARRAY && json_len(entry) > 0)
        return json_copy(json_get(entry, 0));
    return entry ? json_copy(entry) : json_null();
}
json_t *w9_runagentca_634fc9_tuple(const struct json_t *in) {
    const json_t *entry = json_obj_get(in, "entry");
    bool as_tuple = json_get_bool(in, "as_tuple", false);
    if (as_tuple && entry && entry->type == JSON_ARRAY && json_len(entry) > 0)
        return json_copy(json_get(entry, 0));
    return json_null();
}
json_t *w9_runagentca_634fc9_config_sig(const struct json_t *in) {
    const json_t *runtime = json_obj_get(in, "runtime");
    const char *model = json_get_str(in, "model", "");
    const char *api_key = json_get_str(runtime, "api_key", "");
    char key_sig[65] = "";
    if (api_key && api_key[0]) {
        char full[65];
        sha256_hex(api_key, strlen(api_key), full);
        memcpy(key_sig, full, 64);
        key_sig[64] = '\0';
    }
    blob b; b.len = 0; b.buf[0] = '\0';
    puts_b(&b, "[");
    put_json_string(&b, model);
    puts_b(&b, ", "); put_json_string(&b, key_sig);
    puts_b(&b, ", "); put_json_string(&b, json_get_str(runtime, "base_url", ""));
    puts_b(&b, ", "); put_json_string(&b, json_get_str(runtime, "provider", ""));
    puts_b(&b, ", "); put_json_string(&b, json_get_str(runtime, "requested_provider", ""));
    puts_b(&b, ", "); put_json_string(&b, json_get_str(runtime, "api_mode", ""));
    puts_b(&b, ", "); put_sorted_items(&b, json_obj_get(runtime, "capabilities"));
    puts_b(&b, ", "); put_sorted_list(&b, json_obj_get(in, "enabled_toolsets"));
    puts_b(&b, ", "); put_json_string(&b, json_get_str(in, "ephemeral_prompt", ""));
    puts_b(&b, ", "); put_sorted_items(&b, json_obj_get(in, "cache_keys"));
    puts_b(&b, ", "); put_json_string(&b, json_get_str(in, "user_id", ""));
    puts_b(&b, ", "); put_json_string(&b, json_get_str(in, "user_id_alt", ""));
    puts_b(&b, ", "); puts_b(&b, json_get_bool(in, "skip_context_files", false) ? "true" : "false");
    puts_b(&b, "]");
    char full[65];
    sha256_hex(b.buf, b.len, full);
    char out[17];
    memcpy(out, full, 16);
    out[16] = '\0';
    return json_string(out);
}
