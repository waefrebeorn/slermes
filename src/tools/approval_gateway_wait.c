/*
 * approval_gateway_wait.c — C home for upstream Hermes tools/approval_gateway_wait.py.
 *
 * PoP: w15_approvalga_206913_identity_digest @ tools/approval_gateway_wait.py:_identity_digest
 * PoP: w15_approvalga_206913_same_answerers @ tools/approval_gateway_wait.py:_same_answerers
 * PoP: w15_approvalga_206913_hook_choice @ tools/approval_gateway_wait.py:_hook_choice
 * PoP: w15_approvalga_206913_cancel_cause @ tools/approval_gateway_wait.py:_cancel_cause
 *
 * Self-contained SHA-256 (static, w15agw_ prefixed) keeps the gate free of a
 * crypto link dependency. Thread/event state is intentionally not ported: the
 * exported helpers below are the pure decision functions of the module.
 */
#include "noc_w15_approvalga_206913.h"
#include "hermes_json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ---------- minimal SHA-256 ---------- */
typedef struct {
    uint32_t h[8];
    uint64_t bitlen;
    unsigned char buf[64];
    size_t buflen;
} w15agw_sha256_ctx;

static const uint32_t W15AGW_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
#define W15AGW_ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void w15agw_block(w15agw_sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = W15AGW_ROR(w[i-15],7) ^ W15AGW_ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = W15AGW_ROR(w[i-2],17) ^ W15AGW_ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = W15AGW_ROR(e,6) ^ W15AGW_ROR(e,11) ^ W15AGW_ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + W15AGW_K[i] + w[i];
        uint32_t S0 = W15AGW_ROR(a,2) ^ W15AGW_ROR(a,13) ^ W15AGW_ROR(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}
static void w15agw_init(w15agw_sha256_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen=0; c->buflen=0;
}
static void w15agw_update(w15agw_sha256_ctx *c, const unsigned char *data, size_t n) {
    c->bitlen += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->buflen; if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == 64) { w15agw_block(c, c->buf); c->buflen = 0; }
    }
}
static void w15agw_final(w15agw_sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bitlen;
    unsigned char pad = 0x80;
    w15agw_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->buflen != 56) w15agw_update(c, &zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8*i));
    w15agw_update(c, len, 8);
    for (int i = 0; i < 8; ++i) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)c->h[i];
    }
}
static char *w15agw_sha256_hex(const unsigned char *data, size_t n) {
    w15agw_sha256_ctx c; w15agw_init(&c); w15agw_update(&c, data, n);
    unsigned char dig[32]; w15agw_final(&c, dig);
    static const char hx[] = "0123456789abcdef";
    char *out = (char *)malloc(65);
    if (out) {
        for (int i = 0; i < 32; ++i) { out[i*2]=hx[dig[i]>>4]; out[i*2+1]=hx[dig[i]&15]; }
        out[64]='\0';
    }
    return out;
}

char *w15_approvalga_206913_identity_digest(const char *text) {
    const char *s = text ? text : "None";   /* str(None) == "None" */
    return w15agw_sha256_hex((const unsigned char *)s, strlen(s));
}

/* Python truthiness of a decoded JSON value. */
static int py_bool(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

static int jeq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) {
        /* Python True == 1 / False == 0 */
        if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (a->bool_val ? 1 : 0) == (int)b->num_val;
        if (a->type == JSON_NUMBER && b->type == JSON_BOOL) return (int)a->num_val == (b->bool_val ? 1 : 0);
        return 0;
    }
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; ++i) if (!jeq(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; ++i) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !jeq(a->c.items[i], bv)) return 0;
            }
            return 1;
        default: return 0;
    }
}

/* `"*" in (d.get("approvers") or [])` */
static int has_star(const json_t *approvers) {
    if (!py_bool(approvers)) return 0;
    if (approvers->type == JSON_ARRAY) {
        for (size_t i = 0; i < approvers->c.count; ++i) {
            const json_t *v = approvers->c.items[i];
            if (v && v->type == JSON_STRING && !strcmp(v->str_val, "*")) return 1;
        }
        return 0;
    }
    if (approvers->type == JSON_STRING) return strchr(approvers->str_val, '*') != NULL;
    return 0;
}

int w15_approvalga_206913_same_answerers(const json_t *leader, const json_t *follower) {
    if (!leader || !follower) return 0;
    const json_t *la = json_obj_get(leader, "approvers");
    const json_t *fa = json_obj_get(follower, "approvers");
    if (has_star(fa) && has_star(la)) return 1;
    return jeq(json_obj_get(leader, "requester_id"), json_obj_get(follower, "requester_id")) &&
           jeq(la, fa);
}

char *w15_approvalga_206913_hook_choice(int resolved, const json_t *choice, const struct json_t *extra) {
    if (extra && py_bool(json_obj_get(extra, "cancelled"))) return dupstr("cancelled");
    if (!resolved || !py_bool(choice)) return dupstr("timeout");
    if (choice && choice->type == JSON_STRING) return dupstr(choice->str_val);
    if (choice && choice->type == JSON_BOOL) return dupstr(choice->bool_val ? "True" : "False");
    if (choice && choice->type == JSON_NUMBER) {
        char buf[64];
        double d = choice->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else snprintf(buf, sizeof(buf), "%g", d);
        return dupstr(buf);
    }
    return dupstr("timeout");
}

char *w15_approvalga_206913_cancel_cause(const char *state, const struct json_t *entry,
                                         const char *interrupt_reason) {
    if (!state) state = "";
    if (!strcmp(state, "interrupted"))
        return dupstr((interrupt_reason && *interrupt_reason) ? interrupt_reason : "turn interrupted");
    if (!strcmp(state, "set")) {
        const json_t *result = entry ? json_obj_get(entry, "result") : NULL;
        if (!result || result->type == JSON_NULL) {
            const json_t *c = entry ? json_obj_get(entry, "cancelled") : NULL;
            if (c && py_bool(c) && c->type == JSON_STRING) return dupstr(c->str_val);
            return dupstr("the turn ended before the prompt was answered");
        }
    }
    return NULL;
}
