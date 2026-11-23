/*
 * bridged_env.c — C home for upstream Hermes gateway/bridged_env.py.
 *
 * PoP: bridgedenv_digest @ gateway/bridged_env.py:bridged_env_digest
 * PoP: bridgedenv_drop_markers @ gateway/bridged_env.py:drop_bridge_markers
 * PoP: bridgedenv_follow @ gateway/bridged_env.py:follow_bridged_env
 *
 * Self-contained SHA-256 so the gate does not depend on a particular crypto
 * object being linked. All exported symbols are `bridgedenv_` prefixed.
 */
#include "noc_bridgedenv.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- minimal SHA-256 ---------- */
typedef struct {
    uint32_t h[8];
    uint64_t bitlen;
    unsigned char buf[64];
    size_t buflen;
} be_sha256_ctx;

static const uint32_t BE_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
#define BE_ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void be_sha256_block(be_sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = BE_ROR(w[i-15],7) ^ BE_ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = BE_ROR(w[i-2],17) ^ BE_ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = BE_ROR(e,6) ^ BE_ROR(e,11) ^ BE_ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + BE_K[i] + w[i];
        uint32_t S0 = BE_ROR(a,2) ^ BE_ROR(a,13) ^ BE_ROR(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}
static void be_sha256_init(be_sha256_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen=0; c->buflen=0;
}
static void be_sha256_update(be_sha256_ctx *c, const unsigned char *data, size_t n) {
    c->bitlen += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->buflen; if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == 64) { be_sha256_block(c, c->buf); c->buflen = 0; }
    }
}
static void be_sha256_final(be_sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bitlen;
    unsigned char pad = 0x80;
    be_sha256_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->buflen != 56) be_sha256_update(c, &zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8*i));
    be_sha256_update(c, len, 8);
    for (int i = 0; i < 8; ++i) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)c->h[i];
    }
}
static char *be_sha256_hex(const unsigned char *data, size_t n) {
    be_sha256_ctx c; be_sha256_init(&c); be_sha256_update(&c, data, n);
    unsigned char dig[32]; be_sha256_final(&c, dig);
    static const char hx[] = "0123456789abcdef";
    char *out = (char *)malloc(65);
    if (out) {
        for (int i = 0; i < 32; ++i) { out[i*2]=hx[dig[i]>>4]; out[i*2+1]=hx[dig[i]&15]; }
        out[64]='\0';
    }
    return out;
}

/* bridged_env_digest(None) -> None; else "sha256:" + hex */
static char *be_digest_value(const json_t *v) {
    if (!v || v->type != JSON_STRING) return NULL;  /* None -> None */
    char *hex = be_sha256_hex((const unsigned char *)v->str_val, strlen(v->str_val));
    if (!hex) return NULL;
    char *out = (char *)malloc(72);
    if (out) snprintf(out, 72, "sha256:%s", hex);
    free(hex);
    return out;
}

char *bridgedenv_digest(const char *value) {
    if (value == NULL) return NULL;  /* Python None */
    char *hex = be_sha256_hex((const unsigned char *)value, strlen(value));
    if (!hex) return NULL;
    char *out = (char *)malloc(72);
    if (out) snprintf(out, 72, "sha256:%s", hex);
    free(hex);
    return out;
}

/* key.upper().startswith("HERMES_BRIDGED_") (ASCII case-insensitive) */
static int be_is_marker_key(const char *key) {
    static const char *P = "HERMES_BRIDGED_";
    size_t n = strlen(P);
    if (strlen(key) < n) return 0;
    for (size_t i = 0; i < n; ++i)
        if (toupper((unsigned char)key[i]) != (unsigned char)P[i]) return 0;
    return 1;
}

struct json_t *bridgedenv_drop_markers(const struct json_t *env) {
    json_t *out = json_object();
    if (!env || env->type != JSON_OBJECT) return out;
    size_t n = json_object_size(env);
    const char **markers = n ? (const char **)calloc(n, sizeof(char *)) : NULL;
    size_t nm = 0;
    for (size_t i = 0; i < n; ++i) {
        const char *k = json_object_get_key_at(env, i);
        if (k && be_is_marker_key(k)) markers[nm++] = k;
    }
    for (size_t i = 0; i < n; ++i) {
        const char *k = json_object_get_key_at(env, i);
        if (k) json_set(out, k, json_copy(json_object_get_at(env, i)));
    }
    for (size_t i = 0; i < nm; ++i) {
        const char *key = markers[i];
        const char *name = key + strlen("HERMES_BRIDGED_");
        if (*name) {
            char *want = be_digest_value(json_obj_get(env, name));
            json_t *marker_v = json_obj_get(env, key);
            const char *got = (marker_v && marker_v->type == JSON_STRING) ? marker_v->str_val : NULL;
            if (want && got && !strcmp(want, got))
                json_obj_del(out, name);
            free(want);
        }
        json_obj_del(out, key);
    }
    free(markers);
    return out;
}

static int be_strcmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

struct json_t *bridgedenv_follow(const struct json_t *scope,
                                 const struct json_t *live,
                                 const struct json_t *grant) {
    json_t *out = json_object();
    if (!scope || scope->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(scope); ++i) {
        const char *k = json_object_get_key_at(scope, i);
        if (k) json_set(out, k, json_copy(json_object_get_at(scope, i)));
    }
    int grant_none = (!grant || grant->type == JSON_NULL);
    if (grant_none && !json_has(live, "GATEWAY_ALLOW_ALL_USERS"))
        json_obj_del(out, "GATEWAY_ALLOW_ALL_USERS");

    /* names = {key[len(prefix):] for key in (*scope,*live)
     *          if key.startswith(prefix) and key != prefix} -- CASE SENSITIVE */
    static const char *P = "HERMES_BRIDGED_";
    size_t plen = strlen(P);
    size_t cap = 64;
    const char **names = (const char **)calloc(cap, sizeof(char *));
    size_t nn = 0;
    const struct json_t *srcs[2] = { scope, live };
    for (int s = 0; s < 2; ++s) {
        const struct json_t *d = srcs[s];
        if (!d || d->type != JSON_OBJECT) continue;
        for (size_t i = 0; i < json_object_size(d); ++i) {
            const char *k = json_object_get_key_at(d, i);
            if (!k || strncmp(k, P, plen) || !k[plen]) continue;
            const char *name = k + plen;
            int seen = 0;
            for (size_t j = 0; j < nn; ++j) if (!strcmp(names[j], name)) { seen = 1; break; }
            if (!seen) {
                if (nn == cap) { cap *= 2; names = (const char **)realloc(names, cap * sizeof(char *)); }
                names[nn++] = name;
            }
        }
    }
    qsort(names, nn, sizeof(char *), be_strcmp);

    const char **ret_names = nn ? (const char **)calloc(nn, sizeof(char *)) : NULL;
    const char **ret_mark  = nn ? (const char **)calloc(nn, sizeof(char *)) : NULL;
    size_t nr = 0;
    for (size_t i = 0; i < nn; ++i) {
        const char *name = names[i];
        char marker[512];
        snprintf(marker, sizeof(marker), "%s%s", P, name);

        json_t *sc_name = json_obj_get(out, name);
        if (sc_name) {
            char *d1 = be_digest_value(sc_name);  /* digest(scope[name]) */
            json_t *sc_marker = json_obj_get(out, marker);
            const char *raw = (sc_marker && sc_marker->type == JSON_STRING) ? sc_marker->str_val : NULL;
            int diff = (d1 == NULL) != (raw == NULL) || (d1 && raw && strcmp(d1, raw));
            free(d1);
            if (diff) continue;
        }
        json_t *lv_marker = json_obj_get(live, marker);
        if (lv_marker && lv_marker->type != JSON_NULL) {
            char *d1 = be_digest_value(json_obj_get(live, name));
            const char *want = (lv_marker->type == JSON_STRING) ? lv_marker->str_val : NULL;
            if (d1 && want && !strcmp(d1, want)) {
                json_set(out, name, json_copy(json_obj_get(live, name)));
                json_set(out, marker, json_copy(lv_marker));
                continue;
            }
            free(d1);
        }
        if (!json_has(live, name) && !json_has(live, marker)) {
            ret_names[nr] = name;
            ret_mark[nr] = (const char *)strdup(marker);
            nr++;
        }
    }
    for (size_t i = 0; i < nr; ++i) {
        json_obj_del(out, ret_names[i]);
        json_obj_del(out, ret_mark[i]);
        free((void *)ret_mark[i]);
    }
    free(ret_names); free(ret_mark);
    free(names);
    return out;
}
