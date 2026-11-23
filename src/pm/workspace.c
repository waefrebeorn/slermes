/*
 * workspace.c — faithful C11 port of the pure helpers in pm/workspace.py.
 *
 * PoP: w5_workspace_b403c1_member_ignored @ pm/workspace.py:_member_ignored
 * PoP: w5_workspace_b403c1_without_release_age @ pm/workspace.py:_without_release_age
 * PoP: w5_workspace_b403c1_conjuncts @ pm/workspace.py:_conjuncts
 * PoP: w5_workspace_b403c1_member_key @ pm/workspace.py:_member_key
 * PoP: w5_workspace_b403c1_solve_only_for @ pm/workspace.py:_solve_only_for
 *
 * Filesystem/TOML/subprocess work in the upstream module is side-effect bound
 * and is represented by explicit fixture seams here.
 */
#include "noc_w5_workspace_b403c1.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

typedef struct { char **v; size_t n, cap; } w5w_strvec;

static void w5w_push(w5w_strvec *s, char *p) {
    if (s->n >= s->cap) {
        size_t need = s->cap ? s->cap * 2 : 8;
        char **tmp = (char **)realloc(s->v, need * sizeof(char *));
        if (!tmp) { free(p); return; }
        s->v = tmp; s->cap = need;
    }
    s->v[s->n++] = p;
}
static void w5w_free(w5w_strvec *s) {
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v); s->v = NULL; s->n = s->cap = 0;
}
static char *w5w_dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *w5w_strip(const char *s) {
    if (!s) return NULL;
    size_t a = 0, b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return w5w_dupn(s + a, b - a);
}
static int w5w_has(const w5w_strvec *s, const char *x) {
    for (size_t i = 0; i < s->n; i++) if (!strcmp(s->v[i], x)) return 1;
    return 0;
}
static void w5w_sort_unique(w5w_strvec *s) {
    for (size_t i = 1; i < s->n; i++) {
        size_t j = i;
        while (j > 0 && strcmp(s->v[j - 1], s->v[j]) > 0) {
            char *t = s->v[j - 1]; s->v[j - 1] = s->v[j]; s->v[j] = t; j--;
        }
    }
    size_t w = 0;
    for (size_t i = 0; i < s->n; i++) {
        if (w && !strcmp(s->v[w - 1], s->v[i])) { free(s->v[i]); continue; }
        s->v[w++] = s->v[i];
    }
    s->n = w;
}

struct json_t *w5_workspace_b403c1_member_ignored(const struct json_t *names) {
    struct json_t *out = json_array();
    if (!names || names->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(names); i++) {
        const json_t *v = json_get(names, i);
        if (!v || v->type != JSON_STRING) continue;
        const char *s = v->str_val;
        size_t n = strlen(s);
        if (!strcmp(s, ".git") || !strcmp(s, ".venv") || !strcmp(s, "venv") ||
            !strcmp(s, "node_modules") || !strcmp(s, "__pycache__") ||
            (n >= 9 && !strcmp(s + n - 9, ".egg-info")))
            json_append(out, json_string(s));
    }
    return out;
}

struct json_t *w5_workspace_b403c1_without_release_age(const char *text) {
    if (!text) return json_string("");
    size_t n = strlen(text), used = 0, cap = n + 1;
    char *out = (char *)malloc(cap);
    if (!out) return json_string("");
    size_t i = 0;
    while (i < n) {
        int at_start = (i == 0 || text[i - 1] == '\n');
        if (at_start && !strncmp(text + i, "exclude-newer", 13)) {
            size_t j = i + 13;
            while (j < n && isspace((unsigned char)text[j])) j++;
            if (j < n && text[j] == '=') {
                size_t k = j;
                while (k < n && text[k] != '\n') k++;
                if (k < n) { i = k + 1; continue; }
            }
        }
        out[used++] = text[i++];
    }
    out[used] = '\0';
    struct json_t *r = json_string(out);
    free(out);
    return r;
}

/* ---- _conjuncts: top-level `and` terms of a PEP 508 marker ---- */
static char *w5w_ungroup(const char *term) {
    char *t = w5w_strip(term);
    while (t && t[0] == '(' && t[strlen(t) - 1] == ')') {
        size_t len = strlen(t);
        int depth = 0, full = 1;
        char quote = 0;
        for (size_t off = 0; off < len; off++) {
            char c = t[off];
            if (quote) { if (c == quote) quote = 0; }
            else if (c == '\'' || c == '"') quote = c;
            else if (c == '(') depth++;
            else if (c == ')') {
                depth--;
                if (depth == 0 && off != len - 1) { full = 0; break; }
            }
        }
        if (depth || !full || len < 2) break;
        char *inner = w5w_dupn(t + 1, len - 2);
        char *stripped = w5w_strip(inner);
        free(inner); free(t);
        t = stripped;
    }
    return t ? t : w5w_dupn("", 0);
}

struct json_t *w5_workspace_b403c1_conjuncts(const char *marker) {
    w5w_strvec terms = {0};
    if (!marker) marker = "";
    size_t len = strlen(marker);
    int depth = 0;
    char quote = 0;
    size_t start = 0;
    int single = 0;
    for (size_t i = 0; i < len; i++) {
        char c = marker[i];
        if (quote) { if (c == quote) quote = 0; }
        else if (c == '\'' || c == '"') quote = c;
        else if (c == '(') depth++;
        else if (c == ')') depth--;
        else if (depth == 0 && !strncmp(marker + i, " or ", 4)) {
            char *u = w5w_ungroup(marker);
            w5w_push(&terms, u);
            single = 1;
            break;
        } else if (depth == 0 && !strncmp(marker + i, " and ", 5)) {
            w5w_push(&terms, w5w_dupn(marker + start, i - start));
            start = i + 5;
        }
    }
    if (!single) w5w_push(&terms, w5w_dupn(marker + start, len - start));
    for (size_t i = 0; i < terms.n; i++) {
        char *u = w5w_ungroup(terms.v[i]);
        free(terms.v[i]);
        terms.v[i] = u;
    }
    w5w_sort_unique(&terms);
    struct json_t *out = json_array();
    for (size_t i = 0; i < terms.n; i++) json_append(out, json_string(terms.v[i]));
    w5w_free(&terms);
    return out;
}

/* ---- minimal, self-contained SHA-256 (for _member_key) ---- */
typedef struct {
    uint32_t h[8];
    uint64_t bits;
    unsigned char buf[64];
    size_t used;
} w5w_sha256;

static const uint32_t W5W_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
static uint32_t w5w_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void w5w_sha_block(w5w_sha256 *s, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4*i] << 24) | ((uint32_t)p[4*i+1] << 16) |
               ((uint32_t)p[4*i+2] << 8) | (uint32_t)p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t x = w[i-15], y = w[i-2];
        uint32_t s0 = w5w_rotr(x,7) ^ w5w_rotr(x,18) ^ (x >> 3);
        uint32_t s1 = w5w_rotr(y,17) ^ w5w_rotr(y,19) ^ (y >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3];
    uint32_t e = s->h[4], f = s->h[5], g = s->h[6], hh = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = w5w_rotr(e,6) ^ w5w_rotr(e,11) ^ w5w_rotr(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + W5W_K[i] + w[i];
        uint32_t S0 = w5w_rotr(a,2) ^ w5w_rotr(a,13) ^ w5w_rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += hh;
}
static void w5w_sha256_init(w5w_sha256 *s) {
    s->h[0]=0x6a09e667u; s->h[1]=0xbb67ae85u; s->h[2]=0x3c6ef372u; s->h[3]=0xa54ff53au;
    s->h[4]=0x510e527fu; s->h[5]=0x9b05688cu; s->h[6]=0x1f83d9abu; s->h[7]=0x5be0cd19u;
    s->bits = 0; s->used = 0;
}
static void w5w_sha256_update(w5w_sha256 *s, const unsigned char *data, size_t n) {
    s->bits += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - s->used;
        if (take > n) take = n;
        memcpy(s->buf + s->used, data, take);
        s->used += take; data += take; n -= take;
        if (s->used == 64) { w5w_sha_block(s, s->buf); s->used = 0; }
    }
}
static void w5w_sha256_final(w5w_sha256 *s, char hex[65]) {
    uint64_t bits = s->bits;
    unsigned char pad = 0x80;
    w5w_sha256_update(s, &pad, 1);
    unsigned char zero = 0;
    while (s->used != 56) w5w_sha256_update(s, &zero, 1);
    unsigned char finalblock[64];
    memcpy(finalblock, s->buf, 56);
    for (int i = 0; i < 8; i++) finalblock[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
    w5w_sha_block(s, finalblock);
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char)(s->h[i] >> (24 - 8 * j));
            hex[i*8 + j*2] = H[byte >> 4];
            hex[i*8 + j*2 + 1] = H[byte & 15];
        }
    hex[64] = '\0';
}

struct json_t *w5_workspace_b403c1_member_key(const char *resolved) {
    if (!resolved) resolved = "";
    w5w_sha256 sha;
    w5w_sha256_init(&sha);
    w5w_sha256_update(&sha, (const unsigned char *)resolved, strlen(resolved));
    char hex[65];
    w5w_sha256_final(&sha, hex);

    /* pathlib.Path.name on Windows: last component after / or \, trailing
     * separators ignored. */
    size_t plen = strlen(resolved);
    while (plen > 0 && (resolved[plen - 1] == '/' || resolved[plen - 1] == '\\')) plen--;
    size_t begin = plen;
    while (begin > 0 && resolved[begin - 1] != '/' && resolved[begin - 1] != '\\') begin--;
    char *name = w5w_dupn(resolved + begin, plen - begin);
    size_t n = strlen(name);
    char *lower = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)name[i]);
    lower[n] = '\0';
    char *replaced = (char *)malloc(n * 2 + 2);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)lower[i];
        int allowed = ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-');
        if (allowed) replaced[k++] = (char)c;
        else if (k == 0 || replaced[k - 1] != '-') replaced[k++] = '-';
    }
    replaced[k] = '\0';
    char *a = replaced;
    while (*a == '-' || *a == '.') a++;
    char *b = replaced + k;
    while (b > a && (b[-1] == '-' || b[-1] == '.')) b--;
    char *fallback = NULL;
    size_t namelen;
    if (a == b) {
        fallback = (char *)malloc(7);
        strcpy(fallback, "plugin");
        a = fallback;
        namelen = 6;
    } else {
        namelen = (size_t)(b - a);
    }
    size_t total = namelen + 1 + 16 + 1;
    char *out = (char *)malloc(total);
    memcpy(out, a, namelen);
    out[namelen] = '-';
    memcpy(out + namelen + 1, hex, 16);
    out[total - 1] = '\0';
    free(lower); free(replaced); free(fallback); free(name);
    struct json_t *r = json_string(out);
    free(out);
    return r;
}

/* Python repr() of a string, for the marker host expression. */
static char *w5w_py_repr(const char *s) {
    if (!s) s = "";
    int dq = (strchr(s, '\'') && !strchr(s, '"'));
    char q = dq ? '"' : '\'';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 5 + 3);
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == (unsigned char)q) { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++]='\\'; out[k++]='n'; }
        else if (c == '\r') { out[k++]='\\'; out[k++]='r'; }
        else if (c == '\t') { out[k++]='\\'; out[k++]='t'; }
        else if (c < 0x20 || c == 0x7f) {
            static const char *H = "0123456789abcdef";
            out[k++]='\\'; out[k++]='x'; out[k++]=H[c >> 4]; out[k++]=H[c & 15];
        } else out[k++] = (char)c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}

struct json_t *w5_workspace_b403c1_solve_only_for(const struct json_t *environments,
                                                  const struct json_t *target) {
    const char *sysplat = target ? json_get_str(target, "sys_platform", "") : "";
    const char *machine = target ? json_get_str(target, "platform_machine", "") : "";
    char *r1 = w5w_py_repr(sysplat);
    char *r2 = w5w_py_repr(machine);
    size_t hostlen = strlen("sys_platform == ") + strlen(r1) + strlen(" and platform_machine == ") + strlen(r2) + 1;
    char *host = (char *)malloc(hostlen);
    snprintf(host, hostlen, "sys_platform == %s and platform_machine == %s", r1, r2);
    free(r1); free(r2);

    struct json_t *result = json_object();
    struct json_t *envs = json_array();
    size_t count = (environments && environments->type == JSON_ARRAY) ? json_len(environments) : 0;
    if (count == 0) {
        json_append(envs, json_string(host));
    } else {
        for (size_t i = 0; i < count; i++) {
            const json_t *e = json_get(environments, i);
            const char *s = (e && e->type == JSON_STRING) ? e->str_val : "";
            size_t len = strlen(s) + strlen(host) + 8;
            char *line = (char *)malloc(len);
            snprintf(line, len, "(%s) and %s", s, host);
            json_append(envs, json_string(line));
            free(line);
        }
    }
    free(host);
    json_set(result, "environments", envs);
    return result;
}

