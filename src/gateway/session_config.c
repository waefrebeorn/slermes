/*
 * session_config.c — C port of upstream Hermes gateway/session_config.py
 * (gate w8_sessionconf_6d4604).
 *
 * PoP: sessionconf_mcp_revision @ gateway/session_config.py:mcp_revision
 * PoP: sessionconf_config_get_problem @ gateway/session_config.py:config_get
 * PoP: sessionconf_model_options_problem @ gateway/session_config.py:model_options
 * PoP: sessionconf_fast_value @ gateway/session_config.py:_fast
 */
#include "noc_w8_sessionconf_6d4604.h"
#include "hermes_json.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Python-compatible json.dumps(sort_keys=True) ---------- */
typedef struct { char *p; size_t n, cap; } BUF;
static void bput(BUF *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 128;
        while (nc < b->n + n + 1) nc *= 2;
        b->p = (char *)realloc(b->p, nc);
        b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}
static void bputs(BUF *b, const char *s) { bput(b, s, strlen(s)); }

static void py_quote(BUF *b, const char *s) {
    bputs(b, "\"");
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; ) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { char x[2] = {'\\', (char)c}; bput(b, x, 2); p++; }
        else if (c == '\n') { bputs(b, "\\n"); p++; }
        else if (c == '\r') { bputs(b, "\\r"); p++; }
        else if (c == '\t') { bputs(b, "\\t"); p++; }
        else if (c == '\b') { bputs(b, "\\b"); p++; }
        else if (c == '\f') { bputs(b, "\\f"); p++; }
        else if (c < 0x20) { char x[8]; snprintf(x, sizeof(x), "\\u%04x", (unsigned)c); bputs(b, x); p++; }
        else if (c < 0x80) { bput(b, (const char *)&c, 1); p++; }
        else {
            unsigned int cp; int len;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; len = 2; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; len = 3; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; len = 4; }
            else { cp = 0xFFFDu; len = 1; }
            for (int i = 1; i < len && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3Fu);
            p += len;
            char x[16];
            if (cp <= 0xFFFF) snprintf(x, sizeof(x), "\\u%04x", cp);
            else { cp -= 0x10000; snprintf(x, sizeof(x), "\\u%04x\\u%04x",
                                           0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF)); }
            bputs(b, x);
        }
    }
    bputs(b, "\"");
}
static int cmp_strptr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static void py_dump(BUF *b, const json_t *v, int depth) {
    if (!v || v->type == JSON_NULL) { bputs(b, "null"); return; }
    if (v->type == JSON_BOOL) { bputs(b, v->bool_val ? "true" : "false"); return; }
    if (v->type == JSON_STRING) { py_quote(b, v->str_val); return; }
    if (v->type == JSON_NUMBER) {
        char x[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(x, sizeof(x), "%lld", (long long)d);
        else snprintf(x, sizeof(x), "%.17g", d);
        bputs(b, x);
        return;
    }
    if (v->type == JSON_ARRAY) {
        bputs(b, "[");
        for (size_t i = 0; i < json_len(v); i++) {
            if (i) bputs(b, ", ");
            py_dump(b, json_get(v, i), depth + 1);
        }
        bputs(b, "]");
        return;
    }
    size_t n = json_object_size(v);
    const char **keys = (const char **)malloc((n ? n : 1) * sizeof(const char *));
    for (size_t i = 0; i < n; i++) keys[i] = json_object_get_key_at(v, i);
    qsort(keys, n, sizeof(const char *), cmp_strptr);
    bputs(b, "{");
    for (size_t i = 0; i < n; i++) {
        if (i) bputs(b, ", ");
        py_quote(b, keys[i]);
        bputs(b, ": ");
        py_dump(b, json_obj_get(v, keys[i]), depth + 1);
    }
    bputs(b, "}");
    free(keys);
}

/* ---------- compact SHA-256 ---------- */
static const uint32_t K256[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
typedef struct { uint32_t h[8]; uint64_t total; unsigned char buf[64]; size_t blen; } SHA;
static uint32_t ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void sha_block(SHA *s, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[4*i]<<24)|((uint32_t)p[4*i+1]<<16)|((uint32_t)p[4*i+2]<<8)|p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t x = w[i-15], y = w[i-2];
        uint32_t s0 = ror32(x,7)^ror32(x,18)^(x>>3);
        uint32_t s1 = ror32(y,17)^ror32(y,19)^(y>>10);
        w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror32(e,6)^ror32(e,11)^ror32(e,25);
        uint32_t ch = (e&f)^((~e)&g);
        uint32_t t1 = h+S1+ch+K256[i]+w[i];
        uint32_t S0 = ror32(a,2)^ror32(a,13)^ror32(a,22);
        uint32_t maj = (a&b)^(a&c)^(b&c);
        uint32_t t2 = S0+maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d;
    s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}
static void sha_init(SHA *s) {
    s->h[0]=0x6a09e667; s->h[1]=0xbb67ae85; s->h[2]=0x3c6ef372; s->h[3]=0xa54ff53a;
    s->h[4]=0x510e527f; s->h[5]=0x9b05688c; s->h[6]=0x1f83d9ab; s->h[7]=0x5be0cd19;
    s->total=0; s->blen=0;
}
static void sha_update(SHA *s, const void *data, size_t n) {
    const unsigned char *p=(const unsigned char*)data;
    s->total += n;
    while (n) {
        size_t take = 64 - s->blen;
        if (take > n) take = n;
        memcpy(s->buf + s->blen, p, take);
        s->blen += take; p += take; n -= take;
        if (s->blen == 64) { sha_block(s, s->buf); s->blen = 0; }
    }
}
static void sha_final(SHA *s, unsigned char out[32]) {
    uint64_t bits = s->total * 8;
    unsigned char pad[72];
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    size_t z = (s->blen < 56) ? (56 - s->blen) : (120 - s->blen);
    sha_update(s, pad, z);
    for (int i = 0; i < 8; i++) pad[i] = (unsigned char)(bits >> (56 - 8*i));
    sha_update(s, pad, 8);
    for (int i = 0; i < 8; i++) {
        out[4*i]   = (unsigned char)(s->h[i] >> 24);
        out[4*i+1] = (unsigned char)(s->h[i] >> 16);
        out[4*i+2] = (unsigned char)(s->h[i] >> 8);
        out[4*i+3] = (unsigned char)(s->h[i]);
    }
}

json_t *sessionconf_mcp_revision(const json_t *source) {
    json_t *d = json_object();
    const char *keys[3] = {"mcp", "mcp_servers", "tools"};
    for (int i = 0; i < 3; i++) {
        json_t *v = json_obj_get(source, keys[i]);
        json_set(d, keys[i], v ? json_copy(v) : json_null());
    }
    BUF b = {0};
    py_dump(&b, d, 0);
    SHA s; sha_init(&s);
    sha_update(&s, b.p, b.n);
    unsigned char dig[32]; sha_final(&s, dig);
    char hex[16];
    for (int i = 0; i < 6; i++) snprintf(hex + 2*i, 3, "%02x", dig[i]);
    hex[12] = '\0';
    json_free(d);
    free(b.p);
    return json_string(hex);
}

static int in_set(const char *v, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strcmp(v, set[i])) return 1;
    return 0;
}

json_t *sessionconf_config_get_problem(const json_t *params) {
    static const char *const allowed[] = {"session_id", "profile", "key", "cwd"};
    if (!json_is_object(params)) return json_string("invalid_params");
    for (size_t i = 0; i < json_object_size(params); i++) {
        const char *k = json_object_get_key_at(params, i);
        if (!k || !in_set(k, allowed, sizeof(allowed)/sizeof(allowed[0])))
            return json_string("invalid_params");
    }
    json_t *keyv = json_obj_get(params, "key");
    if (!keyv || keyv->type != JSON_STRING) return json_string("invalid_params");
    const char *key = keyv->str_val;
    static const char *const busy[] = {"busy", "verbose", "yolo", "show_reasoning"};
    if (in_set(key, busy, sizeof(busy)/sizeof(busy[0]))) return json_string("busy");
    json_t *cwd = json_obj_get(params, "cwd");
    if (cwd && (strcmp(key, "project") || cwd->type != JSON_STRING))
        return json_string("invalid_params");
    return json_string("ok");
}

json_t *sessionconf_model_options_problem(const json_t *params) {
    static const char *const flags[] = {"explicit_only", "include_unconfigured", "refresh"};
    static const char *const allowed[] = {"explicit_only", "include_unconfigured",
                                          "refresh", "profile", "session_id"};
    if (!json_is_object(params)) return json_string("invalid_params");
    for (size_t i = 0; i < json_object_size(params); i++) {
        const char *k = json_object_get_key_at(params, i);
        if (!k || !in_set(k, allowed, sizeof(allowed)/sizeof(allowed[0])))
            return json_string("invalid_params");
    }
    for (size_t i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
        json_t *v = json_obj_get(params, flags[i]);
        if (v && v->type != JSON_BOOL) return json_string("invalid_params");
    }
    return json_string("ok");
}

json_t *sessionconf_fast_value(const char *tier) {
    if (tier && !strcmp(tier, "priority")) return json_string("fast");
    if (tier && !strcmp(tier, "ultrafast")) return json_string("ultrafast");
    return json_string("normal");
}
