/*
 * environments.c — C home for upstream Hermes pm/environments.py.
 *
 * PoP: w5_environments_407599_venv_bin_dir @ pm/environments.py:venv_bin_dir
 * PoP: w5_environments_407599_venv_python @ pm/environments.py:venv_python
 * PoP: w5_environments_407599_fish_quote @ pm/environments.py:_fish_quote
 * PoP: w5_environments_407599_shell_exports @ pm/environments.py:shell_exports
 * PoP: w5_environments_407599_install_key @ pm/environments.py:install_key
 *
 * Paths are emitted with '/' separators (the oracle normalises platform
 * separators); an explicit `windows` argument replaces os.name. All exported
 * symbols are w5_environments_407599_ prefixed.
 */
#include "noc_w5_environments_407599.h"
#include "hermes_json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- small string helpers ---------------- */
static char *env_strdup_n(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* Join base + "/" + leaf, stripping trailing separators from base. */
static char *env_join(const char *base, const char *leaf) {
    if (!base) base = "";
    size_t n = strlen(base);
    while (n > 1 && base[n - 1] == '/') --n;
    char *o = (char *)malloc(n + 1 + strlen(leaf) + 1);
    if (!o) return NULL;
    memcpy(o, base, n);
    size_t p = n;
    if (n && o[n - 1] != '/') o[p++] = '/';
    strcpy(o + p, leaf);
    return o;
}

static const char *env_basename(const char *path) {
    if (!path) return "";
    const char *s = strrchr(path, '/');
    const char *b = strrchr(path, '\\');
    if (b && (!s || b > s)) s = b;
    return s ? s + 1 : path;
}

char *w5_environments_407599_venv_bin_dir(const char *venv, int windows) {
    if (windows < 0) windows = 1;   /* host is Windows: os.name == "nt" */
    return env_join(venv ? venv : "", windows ? "Scripts" : "bin");
}

char *w5_environments_407599_venv_python(const char *venv, int windows) {
    char *bin = w5_environments_407599_venv_bin_dir(venv, windows);
    if (!bin) return NULL;
    const char *leaf = !strcmp(env_basename(bin), "Scripts") ? "python.exe" : "python";
    char *out = env_join(bin, leaf);
    free(bin);
    return out;
}

/* ---------------- quoting ---------------- */
char *w5_environments_407599_fish_quote(const char *value) {
    if (!value) value = "";
    size_t cap = strlen(value) * 2 + 4;
    char *o = (char *)malloc(cap);
    if (!o) return NULL;
    size_t p = 0;
    o[p++] = '\'';
    for (const char *s = value; *s; ++s) {
        if (*s == '\\') o[p++] = '\\';
        else if (*s == '\'') o[p++] = '\\';
        o[p++] = *s;
    }
    o[p++] = '\'';
    o[p] = '\0';
    return o;
}

/* shlex.quote for a non-empty string. */
static char *env_shlex_quote(const char *s) {
    if (!s || !*s) return env_strdup_n("''", 2);
    int safe = 1;
    for (const char *p = s; *p; ++p) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || strchr("_@%+=:,./-", c))) { safe = 0; break; }
    }
    if (safe) return env_strdup_n(s, strlen(s));
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 3);
    if (!o) return NULL;
    size_t p = 0;
    o[p++] = '\'';
    for (const char *q = s; *q; ++q) {
        if (*q == '\'') { o[p++] = '\''; o[p++] = '"'; o[p++] = '\''; o[p++] = '"'; o[p++] = '\''; }
        else o[p++] = *q;
    }
    o[p++] = '\'';
    o[p] = '\0';
    return o;
}

static int isalpha_local(char c);
static int env_is_identifier(const char *s) {
    if (!s || !(isalpha_local(s[0]) || s[0] == '_')) return 0;
    for (const char *p = s + 1; *p; ++p)
        if (!(isalpha_local(*p) || (*p >= '0' && *p <= '9') || *p == '_')) return 0;
    return 1;
}
/* ASCII-only letter check kept local (no locale dependence). */
static int isalpha_local(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int env_fish_readonly(const char *name) {
    static const char *const ro[] = {
        "PWD", "SHLVL", "_", "status", "version", "hostname", "fish_pid", "history",
        "pipestatus", "status_generation", "umask", "FISH_VERSION"
    };
    for (size_t i = 0; i < sizeof(ro) / sizeof(ro[0]); ++i)
        if (!strcmp(name, ro[i])) return 1;
    return 0;
}

/* str() coercion of a JSON value (fixtures mostly carry strings). */
static char *env_py_str(const json_t *v) {
    if (!v) return env_strdup_n("None", 4);
    if (v->type == JSON_STRING) return env_strdup_n(v->str_val, strlen(v->str_val));
    if (v->type == JSON_BOOL) return env_strdup_n(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (v->type == JSON_NULL) return env_strdup_n("None", 4);
    if (v->type == JSON_NUMBER) {
        char b[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(b, sizeof(b), "%lld", (long long)d);
        else snprintf(b, sizeof(b), "%g", d);
        return env_strdup_n(b, strlen(b));
    }
    char *s = json_dumps(v, 0);
    return s ? s : env_strdup_n("", 0);
}

char *w5_environments_407599_shell_exports(const struct json_t *env, const char *dialect) {
    int fish = dialect && !strcmp(dialect, "fish");
    if (!dialect || (!fish && strcmp(dialect, "sh"))) return env_strdup_n("", 0);
    size_t out_cap = 256, out_len = 0;
    char *out = (char *)malloc(out_cap);
    if (!out) return NULL;
    out[0] = '\0';
    if (!env || env->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(env); ++i) {
        const char *name = json_object_get_key_at(env, i);
        if (!name || !env_is_identifier(name)) continue;
        if (fish && env_fish_readonly(name)) continue;
        char *val = env_py_str(json_object_get_at(env, i));
        char *quoted = fish ? w5_environments_407599_fish_quote(val) : env_shlex_quote(val);
        free(val);
        if (!quoted) continue;
        const char *prefix = fish ? "set -gx " : "export ";
        size_t need = out_len + strlen(prefix) + strlen(name) + 1 + strlen(quoted) + 2;
        if (need > out_cap) {
            while (need > out_cap) out_cap *= 2;
            out = (char *)realloc(out, out_cap);
        }
        if (out_len) out[out_len++] = '\n';
        if (fish)
            out_len += (size_t)snprintf(out + out_len, out_cap - out_len, "%s%s %s",
                                        prefix, name, quoted);
        else
            out_len += (size_t)snprintf(out + out_len, out_cap - out_len, "%s%s=%s",
                                        prefix, name, quoted);
        free(quoted);
    }
    out[out_len] = '\0';
    return out;
}

/* ---------------- SHA-256 (self-contained, for install_key) ---------------- */
typedef struct { uint32_t h[8]; uint64_t bitlen; unsigned char buf[64]; size_t buflen; } env_sha;
static const uint32_t ENV_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
#define ENV_ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void env_sha_block(env_sha *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) | ((uint32_t)p[i*4+2] << 8) | p[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ENV_ROR(w[i-15],7) ^ ENV_ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ENV_ROR(w[i-2],17) ^ ENV_ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = ENV_ROR(e,6) ^ ENV_ROR(e,11) ^ ENV_ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + ENV_K[i] + w[i];
        uint32_t S0 = ENV_ROR(a,2) ^ ENV_ROR(a,13) ^ ENV_ROR(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}
static void env_sha_update(env_sha *c, const unsigned char *data, size_t n) {
    c->bitlen += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->buflen; if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == 64) { env_sha_block(c, c->buf); c->buflen = 0; }
    }
}
static char *env_install_key(const char *s) {
    env_sha c;
    memset(&c, 0, sizeof(c));
    c.h[0]=0x6a09e667u; c.h[1]=0xbb67ae85u; c.h[2]=0x3c6ef372u; c.h[3]=0xa54ff53au;
    c.h[4]=0x510e527fu; c.h[5]=0x9b05688cu; c.h[6]=0x1f83d9abu; c.h[7]=0x5be0cd19u;
    size_t n = s ? strlen(s) : 0;
    env_sha_update(&c, (const unsigned char *)(s ? s : ""), n);
    uint64_t bits = c.bitlen;
    unsigned char one = 0x80; env_sha_update(&c, &one, 1);
    unsigned char zero = 0; while (c.buflen != 56) env_sha_update(&c, &zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8*i));
    env_sha_update(&c, len, 8);
    unsigned char dig[32];
    for (int i = 0; i < 8; ++i) {
        dig[i*4]   = (unsigned char)(c.h[i] >> 24);
        dig[i*4+1] = (unsigned char)(c.h[i] >> 16);
        dig[i*4+2] = (unsigned char)(c.h[i] >> 8);
        dig[i*4+3] = (unsigned char)c.h[i];
    }
    static const char hx[] = "0123456789abcdef";
    char *out = (char *)malloc(17);
    if (out) {
        for (int i = 0; i < 8; ++i) { out[i*2]=hx[dig[i]>>4]; out[i*2+1]=hx[dig[i]&15]; }
        out[16] = '\0';
    }
    return out;
}
char *w5_environments_407599_install_key_ex(const char *canonical) {
    return env_install_key(canonical ? canonical : "");
}

