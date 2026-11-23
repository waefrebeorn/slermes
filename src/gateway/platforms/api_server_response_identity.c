/*
 * api_server_response_identity.c — C home for upstream
 * gateway/platforms/api_server_response_identity.py.
 *
 * PoP: apid_durable_response_id @ gateway/platforms/api_server_response_identity.py:durable_response_id
 * PoP: apid_next_item_id @ gateway/platforms/api_server_response_identity.py:next_item_id
 * PoP: apid_identify_output @ gateway/platforms/api_server_response_identity.py:identify_output
 *
 * Self-contained: a minimal SHA-256 plus a Python-json.dumps-compatible
 * serializer for the compact `[response_id, kind, call_id, commentary, n]`
 * payload that feeds next_item_id's digest.  No external crypto dependency.
 */
#include "noc_apid.h"

#include "hermes_json.h"

#include <stdio.h>
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
    uint32_t a=c->h[0],b=c->h[1],cw=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h7=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h7 + S1 + ch + K256[i] + w[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
        uint32_t maj = (a & b) ^ (a & cw) ^ (b & cw);
        uint32_t t2 = S0 + maj;
        h7=g; g=f; f=e; e=d+t1; d=cw; cw=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cw; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h7;
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
static char *sha256_hex(const unsigned char *data, size_t n) {
    sha256_ctx c; sha256_init(&c); sha256_update(&c, data, n);
    unsigned char dig[32]; sha256_final(&c, dig);
    static const char hx[] = "0123456789abcdef";
    char *out = (char *)malloc(65);
    if (out) { for (int i = 0; i < 32; ++i) { out[i*2]=hx[dig[i]>>4]; out[i*2+1]=hx[dig[i]&15]; } out[64]='\0'; }
    return out;
}

/* ---------- Python json.dumps-compatible string escaping ---------- */
typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 128;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = '\0';
}
static void bputs(buf_t *b, const char *s) { bput(b, s, strlen(s)); }
static void buhex4(buf_t *b, unsigned cp) {
    static const char hx[] = "0123456789abcdef";
    char e[6] = { '\\','u', hx[(cp>>12)&15], hx[(cp>>8)&15], hx[(cp>>4)&15], hx[cp&15] };
    bput(b, e, 6);
}
static void py_json_string(buf_t *b, const char *s) {
    bputs(b, "\"");
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) {
        unsigned cp = *p; size_t adv = 1;
        if (cp >= 0xf0) { cp = ((p[0]&7)<<18)|((p[1]&63)<<12)|((p[2]&63)<<6)|(p[3]&63); adv=4; }
        else if (cp >= 0xe0) { cp = ((p[0]&15)<<12)|((p[1]&63)<<6)|(p[2]&63); adv=3; }
        else if (cp >= 0xc0) { cp = ((p[0]&31)<<6)|(p[1]&63); adv=2; }
        p += adv;
        switch (cp) {
            case '"':  bputs(b, "\\\""); break;
            case '\\': bputs(b, "\\\\"); break;
            case '\b': bputs(b, "\\b"); break;
            case '\f': bputs(b, "\\f"); break;
            case '\n': bputs(b, "\\n"); break;
            case '\r': bputs(b, "\\r"); break;
            case '\t': bputs(b, "\\t"); break;
            default:
                if (cp < 0x20 || cp > 0x7f) {
                    if (cp <= 0xffff) buhex4(b, cp);
                    else { cp -= 0x10000; buhex4(b, 0xd800 + (cp >> 10)); buhex4(b, 0xdc00 + (cp & 0x3ff)); }
                } else { char c = (char)cp; bput(b, &c, 1); }
        }
    }
    bputs(b, "\"");
}

/* ---------- next_item_id ---------- */
typedef struct {
    char kind[64];
    char call_id[256];
    int  has_call;
    int  commentary;
    long long count;
} counter_t;

static const char *prefix_for(const char *kind) {
    if (!strcmp(kind, "function_call")) return "fc";
    if (!strcmp(kind, "function_call_output")) return "fco";
    if (!strcmp(kind, "reasoning")) return "rs";
    if (!strcmp(kind, "message")) return "msg";
    return "";
}

static char *next_item_id(const char *response_id, counter_t *cs, size_t *ncs,
                          const char *kind, const char *call_id, int commentary) {
    long long occ = 0;
    for (size_t i = 0; i < *ncs; ++i) {
        if (!strcmp(cs[i].kind, kind) && cs[i].has_call == (call_id != NULL) &&
            (!call_id || !strcmp(cs[i].call_id, call_id)) &&
            cs[i].commentary == commentary) { occ = cs[i].count; break; }
    }
    int found = 0;
    for (size_t i = 0; i < *ncs; ++i) {
        if (!strcmp(cs[i].kind, kind) && cs[i].has_call == (call_id != NULL) &&
            (!call_id || !strcmp(cs[i].call_id, call_id)) &&
            cs[i].commentary == commentary) { cs[i].count = occ + 1; found = 1; break; }
    }
    if (!found && *ncs < 128) {
        counter_t *e = &cs[(*ncs)++];
        snprintf(e->kind, sizeof(e->kind), "%s", kind ? kind : "");
        if (call_id) { snprintf(e->call_id, sizeof(e->call_id), "%s", call_id); e->has_call = 1; }
        else { e->call_id[0] = '\0'; e->has_call = 0; }
        e->commentary = commentary; e->count = occ + 1;
    }

    buf_t b = {0};
    bputs(&b, "[");
    py_json_string(&b, response_id);
    bputs(&b, ",");
    py_json_string(&b, kind);
    bputs(&b, ",");
    if (call_id) py_json_string(&b, call_id); else bputs(&b, "null");
    bputs(&b, ",");
    bputs(&b, commentary ? "true" : "false");
    bputs(&b, ",");
    char occs[32]; snprintf(occs, sizeof(occs), "%lld", occ);
    bputs(&b, occs);
    bputs(&b, "]");
    char *hex = sha256_hex((const unsigned char *)b.p, b.n);
    free(b.p);
    if (!hex) return strdup("");
    char *out = (char *)malloc(64);
    if (out) snprintf(out, 64, "%s_%.*s", prefix_for(kind), 24, hex);
    free(hex);
    return out;
}

char *apid_durable_response_id(const char *request_key) {
    char *hex = sha256_hex((const unsigned char *)(request_key ? request_key : ""),
                           request_key ? strlen(request_key) : 0);
    char *out = (char *)malloc(64);
    if (out) snprintf(out, 64, "resp_%.*s", 28, hex ? hex : "");
    free(hex);
    return out;
}

struct json_t *apid_item_id_sequence(const char *response_id, const struct json_t *calls) {
    json_t *arr = json_array();
    counter_t cs[128]; size_t ncs = 0;
    if (!calls || calls->type != JSON_ARRAY) return arr;
    for (size_t i = 0; i < json_len(calls); ++i) {
        json_t *c = json_get(calls, i);
        if (!c || c->type != JSON_OBJECT) { json_append(arr, json_null()); continue; }
        const char *kind = json_get_str(c, "type", json_get_str(c, "kind", ""));
        const char *cid = json_get_str(c, "call_id", NULL);
        int comm = json_is_true(json_obj_get(c, "commentary")) ? 1 : 0;
        char *id = next_item_id(response_id, cs, &ncs, kind, cid, comm);
        json_append(arr, json_string(id ? id : ""));
        free(id);
    }
    return arr;
}

struct json_t *apid_identify_output(const struct json_t *items, const char *response_id) {
    json_t *out = json_array();
    counter_t cs[128]; size_t ncs = 0;
    if (!items || items->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(items); ++i) {
        json_t *item = json_get(items, i);
        if (!item || item->type != JSON_OBJECT) { json_append(out, json_null()); continue; }
        const char *kind = json_get_str(item, "type", "");
        json_t *cidv = json_obj_get(item, "call_id");
        const char *cid = (cidv && cidv->type == JSON_STRING) ? cidv->str_val : NULL;
        int comm = !strcmp(json_get_str(item, "phase", ""), "commentary") ? 1 : 0;
        char *id = next_item_id(response_id, cs, &ncs, kind, cid, comm);
        json_t *copy = json_copy(item);
        json_set(copy, "id", json_string(id ? id : ""));
        json_append(out, copy);
        free(id);
    }
    return out;
}
