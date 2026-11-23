/*
 * mcp_tool_schema.c — C home for upstream Hermes tools/mcp_tool_schema.py.
 *
 * PoP: w8_mcptoolsch_81805c_sanitize_mcp_name_component @ tools/mcp_tool_schema.py:sanitize_mcp_name_component
 * PoP: w8_mcptoolsch_81805c_mcp_prefixed_tool_name @ tools/mcp_tool_schema.py:mcp_prefixed_tool_name
 * PoP: w8_mcptoolsch_81805c_matches_name_filter @ tools/mcp_tool_schema.py:matches_name_filter
 * PoP: w8_mcptoolsch_81805c_rewrite_local_refs @ tools/mcp_tool_schema.py:_rewrite_local_refs
 */
#include "noc_w8_mcptoolsch_81805c.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- minimal SHA-256 (self-contained, from the apid port pattern) ---------- */
typedef struct { uint32_t h[8]; uint64_t bitlen; unsigned char buf[64]; size_t buflen; } sha256_ctx;
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
        w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|(uint32_t)p[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1 = ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1=ROR(e,6)^ROR(e,11)^ROR(e,25), ch=(e&f)^((~e)&g);
        uint32_t t1=hh+S1+ch+K256[i]+w[i];
        uint32_t S0=ROR(a,2)^ROR(a,13)^ROR(a,22), maj=(a&b)^(a&cc)^(b&cc);
        uint32_t t2=S0+maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}
static void sha256_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen=0; c->buflen=0;
}
static void sha256_update(sha256_ctx *c, const unsigned char *data, size_t n) {
    c->bitlen += (uint64_t)n*8;
    while (n) {
        size_t take = 64 - c->buflen; if (take > n) take = n;
        memcpy(c->buf+c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == 64) { sha256_block(c, c->buf); c->buflen = 0; }
    }
}
static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits=c->bitlen; unsigned char pad=0x80, zero=0;
    sha256_update(c,&pad,1);
    while (c->buflen != 56) sha256_update(c,&zero,1);
    unsigned char len[8];
    for (int i=0;i<8;++i) len[i]=(unsigned char)(bits>>(56-8*i));
    sha256_update(c,len,8);
    for (int i=0;i<8;++i) {
        out[i*4]=(unsigned char)(c->h[i]>>24); out[i*4+1]=(unsigned char)(c->h[i]>>16);
        out[i*4+2]=(unsigned char)(c->h[i]>>8); out[i*4+3]=(unsigned char)c->h[i];
    }
}
static void sha256_hex(const unsigned char *data, size_t n, char out[65]) {
    sha256_ctx c; sha256_init(&c); sha256_update(&c,data,n);
    unsigned char dig[32]; sha256_final(&c,dig);
    static const char hx[]="0123456789abcdef";
    for (int i=0;i<32;++i) { out[i*2]=hx[dig[i]>>4]; out[i*2+1]=hx[dig[i]&15]; }
    out[64]='\0';
}


char *w8_mcptoolsch_81805c_sanitize_mcp_name_component(const char *value) {
    const char *s = value ? value : "";
    char *out = (char *)malloc(strlen(s) + 1);  /* one byte per codepoint at most */
    if (!out) return NULL;
    size_t i = 0, o = 0;
    while (s[i]) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = 1;
        if (c >= 0xF0) adv = 4;
        else if (c >= 0xE0) adv = 3;
        else if (c >= 0xC0) adv = 2;
        /* Python re.sub works on codepoints: one replacement per non-ASCII
           codepoint, never one per UTF-8 byte. */
        if (adv == 1 && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') || c == '_'))
            out[o++] = (char)c;
        else
            out[o++] = '_';
        for (size_t k = 0; k < adv && s[i]; ++k) i++;
    }
    out[o] = '\0';
    return out;
}

char *w8_mcptoolsch_81805c_mcp_prefixed_tool_name(const char *server_name, const char *tool_name) {
    char *s = w8_mcptoolsch_81805c_sanitize_mcp_name_component(server_name);
    char *t = w8_mcptoolsch_81805c_sanitize_mcp_name_component(tool_name);
    size_t n = 5 + (s ? strlen(s) : 0) + 2 + (t ? strlen(t) : 0) + 1;
    char *full = (char *)malloc(n);
    if (full) sprintf(full, "mcp__%s__%s", s ? s : "", t ? t : "");
    free(s); free(t);
    if (!full) return NULL;
    if (strlen(full) <= 64) return full;
    char dig[65];
    sha256_hex((const unsigned char *)full, strlen(full), dig);
    char *out = (char *)malloc(65);
    if (out) {
        snprintf(out, 65, "%.*s_%s", 55, full, dig);   /* 55 + 1 + 8 = 64 */
        out[64] = '\0';
    }
    free(full);
    return out;
}

/* fnmatch.fnmatchcase subset: '*', '?', '[seq]', '[!seq]', literal. */
static int glob_match(const char *s, const char *p) {
    if (!*p) return *s == '\0';
    if (*p == '*') {
        while (*p == '*') p++;
        if (!*p) return 1;
        for (const char *q = s;; ++q) {
            if (glob_match(q, p)) return 1;
            if (!*q) return 0;
        }
    }
    if (!*s) return 0;
    if (*p == '?') return glob_match(s + 1, p + 1);
    if (*p == '[') {
        const char *r = p + 1;
        int negate = 0;
        if (*r == '!' || *r == '^') { negate = 1; r++; }
        int matched = 0;
        while (*r && *r != ']') {
            if (r[1] == '-' && r[2] && r[2] != ']') {
                if (*s >= *r && *s <= r[2]) matched = 1;
                r += 3;
            } else {
                if (*s == *r) matched = 1;
                r++;
            }
        }
        if (*r != ']') return 0;  /* malformed class: literal '[' */
        return (negate ? !matched : matched) ? glob_match(s + 1, r + 1) : 0;
    }
    return *s == *p ? glob_match(s + 1, p + 1) : 0;
}

int w8_mcptoolsch_81805c_matches_name_filter(const char *tool_name,
        const struct json_t *patterns) {
    if (!patterns || patterns->type != JSON_ARRAY || !json_len(patterns)) return 0;
    const char *n = tool_name ? tool_name : "";
    for (size_t i = 0; i < json_len(patterns); ++i) {
        const char *pat = json_get(patterns, i)->str_val;
        if (pat && !strcmp(n, pat)) return 1;
    }
    for (size_t i = 0; i < json_len(patterns); ++i) {
        const char *pat = json_get(patterns, i)->str_val;
        if (!pat) continue;
        if (strchr(pat, '*') || strchr(pat, '?') || strchr(pat, '['))
            if (glob_match(n, pat)) return 1;
    }
    return 0;
}

static struct json_t *rewrite_node(const struct json_t *node) {
    if (!node) return json_null();
    if (node->type == JSON_ARRAY) {
        struct json_t *arr = json_array();
        for (size_t i = 0; i < json_len(node); ++i) json_append(arr, rewrite_node(json_get(node, i)));
        return arr;
    }
    if (node->type != JSON_OBJECT) return json_copy(node);
    struct json_t *obj = json_object();
    for (size_t i = 0; i < json_object_size(node); ++i) {
        const char *k = json_object_get_key_at(node, i);
        struct json_t *v = json_object_get_at(node, i);
        int is_map = (!strcmp(k, "properties") || !strcmp(k, "patternProperties")) && v->type == JSON_OBJECT;
        const char *outkey = !strcmp(k, "definitions") ? "$defs" : k;
        if (is_map) {
            struct json_t *m = json_object();
            for (size_t j = 0; j < json_object_size(v); ++j)
                json_set(m, json_object_get_key_at(v, j), rewrite_node(json_object_get_at(v, j)));
            json_set(obj, outkey, m);
        } else {
            json_set(obj, outkey, rewrite_node(v));
        }
    }
    struct json_t *ref = json_obj_get(obj, "$ref");
    if (ref && ref->type == JSON_STRING && ref->str_val && !strncmp(ref->str_val, "#/definitions/", 14)) {
        char *nr = (char *)malloc(strlen(ref->str_val) + 8);
        if (nr) {
            sprintf(nr, "#/$defs/%s", ref->str_val + 14);
            json_set(obj, "$ref", json_string(nr));
            free(nr);
        }
    }
    return obj;
}

struct json_t *w8_mcptoolsch_81805c_rewrite_local_refs(const struct json_t *node) {
    return rewrite_node(node);
}
