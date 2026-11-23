/*
 * skills_tool_dedup.c -- faithful C11 port of upstream Hermes
 * tools/skills_tool_dedup.py (gate w10_skillstool_52fa6b).
 *
 * PoP: w10_skillstool_52fa6b_dedup_skill_view    @ tools/skills_tool_dedup.py:dedup_skill_view
 * PoP: w10_skillstool_52fa6b_reset_skill_view_dedup @ tools/skills_tool_dedup.py:reset_skill_view_dedup
 *
 * The per-task (resolved name, file_path) -> sha256(result) registry, its 200-entry
 * FIFO cap, and the exact json.dumps "unchanged" stub are reproduced. The state
 * lives for the life of the process, matching the module-level Python dict.
 */
#include "noc_w10_skillstool_52fa6b.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEDUP_CAP 200
static const char *DEDUP_MSG =
    "Skill content unchanged since it was loaded earlier in this "
    "conversation — refer to the earlier skill_view result; it is still "
    "current and complete. (Re-issued after context compression, this "
    "returns the full content again.)";

/* ---- full SHA-256 -> 64 lowercase hex chars ---- */
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
static uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void sha_block(uint32_t h[8], const unsigned char b[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)b[4*i] << 24) | ((uint32_t)b[4*i+1] << 16) |
               ((uint32_t)b[4*i+2] << 8) | (uint32_t)b[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i-15],7) ^ rotr32(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i-2],17) ^ rotr32(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=h[0],bb=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + SHA_K[i] + w[i];
        uint32_t S0 = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        uint32_t maj = (a & bb) ^ (a & c) ^ (bb & c);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=c; c=bb; bb=a; a=t1+t2;
    }
    h[0]+=a; h[1]+=bb; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}
static void sha256_hex(const unsigned char *data, size_t len, char out[65]) {
    static const uint32_t init[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u };
    uint32_t h[8]; memcpy(h, init, sizeof h);
    size_t total = len, pos = 0;
    while (len >= 64) { sha_block(h, data + pos); pos += 64; len -= 64; }
    unsigned char block[64];
    memcpy(block, data + pos, len);
    block[len] = 0x80;
    if (len >= 56) {
        memset(block + len + 1, 0, 64 - len - 1);
        sha_block(h, block);
        memset(block, 0, 64);
    } else {
        memset(block + len + 1, 0, 56 - len - 1);
    }
    uint64_t bits = (uint64_t)total * 8u;
    for (int i = 0; i < 8; i++) block[56 + i] = (unsigned char)(bits >> (56 - 8*i));
    sha_block(h, block);
    static const char hex[] = "0123456789abcdef";
    for (int w = 0; w < 8; w++)
        for (int j = 7; j >= 0; j--)
            out[w*8 + (7 - j)] = hex[(h[w] >> (4*j)) & 0xf];
    out[64] = '\0';
}

/* ---- tracker state ---- */
typedef struct { char *task, *resolved, *path, *digest; } ent_t;
static ent_t *g_e; static size_t g_n, g_cap;

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count != 0;
    return 1;
}
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static char *dupstr(const char *s) { size_t n = strlen(s ? s : "") + 1; char *o = (char *)malloc(n); if (o) strcpy(o, s ? s : ""); return o; }

static void ent_free(ent_t *e) { free(e->task); free(e->resolved); free(e->path); free(e->digest); }
static void ent_set(ent_t *e, const char *t, const char *r, const char *p, const char *d) {
    e->task = dupstr(t); e->resolved = dupstr(r); e->path = dupstr(p); e->digest = dupstr(d);
}
static void ent_del(size_t i) {
    ent_free(&g_e[i]);
    if (i + 1 < g_n) memmove(&g_e[i], &g_e[i+1], (g_n - i - 1) * sizeof(ent_t));
    g_n--;
}
static size_t find_ent(const char *task, const char *resolved, const char *path) {
    for (size_t i = 0; i < g_n; i++)
        if (!strcmp(g_e[i].task, task) && !strcmp(g_e[i].resolved, resolved) && !strcmp(g_e[i].path, path))
            return i;
    return (size_t)-1;
}
static void ensure_cap(void) {
    if (g_n < g_cap) return;
    g_cap = g_cap ? g_cap * 2 : 16;
    ent_t *t = (ent_t *)realloc(g_e, g_cap * sizeof(ent_t));
    if (t) g_e = t;
}

json_t *w10_skillstool_52fa6b_dedup_skill_view(const char *task_id, const char *name,
        const char *file_path, const json_t *payload, const char *result) {
    if (!task_id || !*task_id) return json_null();
    if (payload && payload->type == JSON_OBJECT) {
        if (truthy(json_obj_get(payload, "setup_needed"))) return json_null();
        const char *rs = jstr(json_obj_get(payload, "readiness_status"));
        if (!strcmp(rs, "setup_needed")) return json_null();
    }
    const char *resolved = name ? name : "";
    if (payload && payload->type == JSON_OBJECT) {
        json_t *pn = json_obj_get(payload, "name");
        if (truthy(pn)) resolved = jstr(pn);
    }
    const char *path = file_path ? file_path : "";
    const char *res = result ? result : "";
    char dig[65]; sha256_hex((const unsigned char *)res, strlen(res), dig);

    size_t ix = find_ent(task_id, resolved, path);
    if (ix != (size_t)-1 && !strcmp(g_e[ix].digest, dig)) {
        char *resolved_lit = json_escape_string(resolved);
        char *file_lit = json_escape_string(*path ? path : "SKILL.md");
        char *msg_lit = json_escape_string(DEDUP_MSG);
        size_t n = strlen(resolved_lit) + strlen(file_lit) + strlen(msg_lit) + 160;
        char *s = (char *)malloc(n);
        snprintf(s, n,
            "{\"success\": true, \"status\": \"unchanged\", \"name\": %s, \"file\": %s, "
            "\"dedup\": true, \"content_returned\": false, \"message\": %s}",
            resolved_lit, file_lit, msg_lit);
        json_t *out = json_string(s);
        free(s); free(resolved_lit); free(file_lit); free(msg_lit);
        return out;
    }
    if (ix != (size_t)-1) ent_del(ix);  /* cache.pop(key, None) -- reinsert at end */
    ensure_cap();
    ent_set(&g_e[g_n], task_id, resolved, path, dig);
    g_n++;
    /* FIFO eviction per task, keeping the newest 200 entries. */
    for (;;) {
        size_t cnt = 0, oldest = (size_t)-1;
        for (size_t i = 0; i < g_n; i++) if (!strcmp(g_e[i].task, task_id)) {
            if (oldest == (size_t)-1) oldest = i;
            cnt++;
        }
        if (cnt <= DEDUP_CAP || oldest == (size_t)-1) break;
        ent_del(oldest);
    }
    return json_null();
}

json_t *w10_skillstool_52fa6b_reset_skill_view_dedup(const char *task_id, int task_given) {
    if (!task_given || !task_id || !*task_id) {
        for (size_t i = 0; i < g_n; i++) ent_free(&g_e[i]);
        g_n = 0;
    } else {
        for (size_t i = 0; i < g_n;) {
            if (!strcmp(g_e[i].task, task_id)) ent_del(i);
            else i++;
        }
    }
    return json_null();
}
