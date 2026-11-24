/*
 * config_secret_refs.c - C home for upstream Hermes hermes_cli/config_secret_refs.py.
 *
 * PoP: w17_configsecr_80ebc4_env_slug @ hermes_cli/config_secret_refs.py:_env_slug
 * PoP: w17_configsecr_80ebc4_model_api_key_env @ hermes_cli/config_secret_refs.py:model_api_key_env
 * PoP: w17_configsecr_80ebc4_aux_api_key_env @ hermes_cli/config_secret_refs.py:aux_api_key_env
 * PoP: w17_configsecr_80ebc4_digest @ hermes_cli/config_secret_refs.py:_digest
 * PoP: w17_configsecr_80ebc4_provider_name_key_env @ hermes_cli/config_secret_refs.py:provider_name_key_env
 * PoP: w17_configsecr_80ebc4_is_env_reference @ hermes_cli/config_secret_refs.py:is_env_reference
 * PoP: w17_configsecr_80ebc4_canonical_endpoint @ hermes_cli/config_secret_refs.py:_canonical_endpoint
 * PoP: w17_configsecr_80ebc4_endpoint_identity @ hermes_cli/config_secret_refs.py:endpoint_identity
 * PoP: w17_configsecr_80ebc4_endpoint_api_key_env @ hermes_cli/config_secret_refs.py:endpoint_api_key_env
 * PoP: w17_configsecr_80ebc4_dashboard_endpoint_key_env @ hermes_cli/config_secret_refs.py:dashboard_endpoint_key_env
 * PoP: w17_configsecr_80ebc4_block_base_url @ hermes_cli/config_secret_refs.py:_block_base_url
 *
 * SHA-256 and the urlparse subset are implemented locally; the config module's
 * custom_endpoint_key_env is mirrored from config.py.
 */
#include "noc_w17_configsecr_80ebc4.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* ---------------- SHA-256 (FIPS 180-4) ---------------- */
typedef struct { uint32_t h[8]; unsigned char buf[64]; size_t len; uint64_t total; } sha_ctx;
static const uint32_t K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(sha_ctx *c, const unsigned char *p) {
    uint32_t w[64], a,b,d,e,f,g,h,t1,t2,cc,s0,s1,S0,S1,ch,maj;
    int i;
    for (i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|((uint32_t)p[i*4+3]);
    for (i = 16; i < 64; ++i) {
        s0 = ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3);
        s1 = ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3];
    e=c->h[4]; f=c->h[5]; g=c->h[6]; h=c->h[7];
    for (i = 0; i < 64; ++i) {
        S1 = ROR(e,6)^ROR(e,11)^ROR(e,25);
        ch = (e&f)^((~e)&g);
        t1 = h+S1+ch+K256[i]+w[i];
        S0 = ROR(a,2)^ROR(a,13)^ROR(a,22);
        maj = (a&b)^(a&cc)^(b&cc);
        t2 = S0+maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}
static void sha_init(sha_ctx *c) {
    static const uint32_t iv[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                                   0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    memcpy(c->h, iv, sizeof(iv));
    c->len = 0; c->total = 0;
}
static void sha_update(sha_ctx *c, const unsigned char *p, size_t n) {
    size_t take;
    c->total += (uint64_t)n;
    while (n) {
        take = 64 - c->len;
        if (take > n) take = n;
        memcpy(c->buf + c->len, p, take);
        c->len += take; p += take; n -= take;
        if (c->len == 64) { sha_block(c, c->buf); c->len = 0; }
    }
}
static void sha_final(sha_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->total * 8;
    unsigned char tail[72];
    size_t pad = (c->len < 56) ? (56 - c->len) : (120 - c->len);
    size_t i;
    memset(tail, 0, sizeof(tail));
    tail[0] = 0x80;
    sha_update(c, tail, pad);
    for (i = 0; i < 8; ++i) tail[i] = (unsigned char)(bits >> (56 - 8*i));
    sha_update(c, tail, 8);
    for (i = 0; i < 8; ++i) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)(c->h[i]);
    }
}

/* hashlib.sha256(text.encode('utf-8')).hexdigest()[:8].upper() */
json_t *w17_configsecr_80ebc4_digest(const char *text) {
    sha_ctx c;
    unsigned char out[32];
    static const char hex[] = "0123456789ABCDEF";
    char res[9];
    int i;
    sha_init(&c);
    sha_update(&c, (const unsigned char *)(text ? text : ""), strlen(text ? text : ""));
    sha_final(&c, out);
    for (i = 0; i < 4; ++i) { res[2*i] = hex[out[i] >> 4]; res[2*i+1] = hex[out[i] & 15]; }
    res[8] = '\0';
    return json_string(res);
}

static char *digest_str(const char *text) {
    json_t *v = w17_configsecr_80ebc4_digest(text);
    const char *s = v && v->type == JSON_STRING ? v->str_val : "";
    char *d = dupstr(s);
    json_free(v);
    return d;
}

/* re.sub(r"[^A-Z0-9]+", "_", upper).strip("_") */
static char *env_slug_str(const char *text) {
    const char *s = text ? text : "";
    size_t n = strlen(s), i, p = 0;
    char *tmp = (char *)malloc(n + 1), *out;
    if (!tmp) return dupstr("");
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
        tmp[i] = (char)c;
    }
    tmp[n] = '\0';
    out = (char *)malloc(n + 2);
    if (!out) { free(tmp); return dupstr(""); }
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)tmp[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (ok) out[p++] = (char)c;
        else if (p > 0 && out[p-1] != '_') out[p++] = '_';
    }
    free(tmp);
    while (p > 0 && out[p-1] == '_') p--;
    i = 0;
    while (i < p && out[i] == '_') i++;
    memmove(out, out + i, p - i);
    out[p - i] = '\0';
    return out;
}

json_t *w17_configsecr_80ebc4_env_slug(const char *text) {
    char *s = env_slug_str(text);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

json_t *w17_configsecr_80ebc4_model_api_key_env(void) {
    return json_string("HERMES_MODEL_API_KEY");
}

/* AUXILIARY_<TASK>_API_KEY with a hash for non plain-lowercase task keys. */
json_t *w17_configsecr_80ebc4_aux_api_key_env(const char *task) {
    const char *t = task ? task : "";
    char *slug, *dig, *res;
    size_t n = strlen(t), i, p = 0;
    int plain = n > 0;
    json_t *r;
    if (!strcmp(t, "delegation")) return json_string("HERMES_DELEGATION_API_KEY");
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)t[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) { plain = 0; break; }
    }
    slug = (char *)malloc(2*n + 16);
    if (!slug) return json_string("");
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)t[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') slug[p++] = (char)c;
        else if (p > 0 && slug[p-1] != '_') slug[p++] = '_';
    }
    slug[p] = '\0';
    if (!plain) {
        dig = digest_str(t);
        snprintf(slug + p, 2*n + 16 - p, "_%s", dig ? dig : "");
        free(dig);
    }
    res = (char *)malloc(strlen(slug) + 24);
    if (!res) { free(slug); return json_string(""); }
    snprintf(res, strlen(slug) + 24, "AUXILIARY_%s_API_KEY", slug);
    free(slug);
    r = json_string(res);
    free(res);
    return r;
}

json_t *w17_configsecr_80ebc4_provider_name_key_env(const char *name) {
    char *slug = env_slug_str(name);
    char *dig = digest_str(name ? name : "");
    char *res = (char *)malloc(strlen(slug ? slug : "") + strlen(dig ? dig : "") + 48);
    json_t *r;
    if (!res) { free(slug); free(dig); return json_string(""); }
    snprintf(res, strlen(slug ? slug : "") + strlen(dig ? dig : "") + 48,
             "HERMES_PROVIDER_%s_%s_API_KEY", (slug && *slug) ? slug : "CUSTOM", dig ? dig : "");
    free(slug); free(dig);
    r = json_string(res);
    free(res);
    return r;
}

/* ^\$\{[^}]+\}$
 * strip() then: starts "${", ends "}", at least one inner char, no inner '}'. */
json_t *w17_configsecr_80ebc4_is_env_reference(const char *value) {
    const char *a = value ? value : "", *b;
    size_t n;
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    if (n < 4 || a[0] != '$' || a[1] != '{' || b[-1] != '}') return json_bool(false);
    if (memchr(a + 2, '}', n - 3) != NULL) return json_bool(false);
    return json_bool(true);
}

typedef struct {
    char scheme[64];
    char netloc[1024];
    char host[1024];
    char path[2048];
    int has_port;
    int port;
} Url41;

/* urlparse subset: only parse netloc when a scheme is present. */
static void parse41(const char *raw, Url41 *u) {
    char *main, *q, *hsh, *sep, *rest, *ap, *slash, *at, *colon, *closeb;
    char hostbuf[1024] = {0};
    const char *a = raw ? raw : "", *b;
    size_t mainlen, i;
    memset(u, 0, sizeof(*u));
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    mainlen = (size_t)(b - a);
    main = (char *)malloc(mainlen + 1);
    if (!main) return;
    memcpy(main, a, mainlen);
    main[mainlen] = '\0';
    q = strchr(main, '?');
    if (q) *q = '\0';
    hsh = strchr(main, '#');
    if (hsh) *hsh = '\0';
    sep = strstr(main, "://");
    if (sep) {
        size_t sl = (size_t)(sep - main);
        snprintf(u->scheme, sizeof(u->scheme), "%.*s", (int)sl, main);
        rest = sep + 3;
        ap = rest;
        slash = strchr(ap, '/');
        if (slash) {
            snprintf(u->netloc, sizeof(u->netloc), "%.*s", (int)(slash - ap), ap);
            snprintf(u->path, sizeof(u->path), "%s", slash);
        } else {
            snprintf(u->netloc, sizeof(u->netloc), "%s", ap);
            u->path[0] = '\0';
        }
    } else {
        /* urlparse treats a schemeless string as a path, not an authority. */
        snprintf(u->path, sizeof(u->path), "%s", main);
    }
    at = u->netloc[0] ? strrchr(u->netloc, '@') : NULL;
    if (u->netloc[0]) {
        char *hp = at ? at + 1 : u->netloc;
        if (*hp == '[') {
            closeb = strchr(hp, ']');
            if (closeb) snprintf(hostbuf, sizeof(hostbuf), "%.*s", (int)(closeb - hp - 1), hp + 1);
            else snprintf(hostbuf, sizeof(hostbuf), "%s", hp);
        } else {
            colon = strrchr(hp, ':');
            if (colon) {
                snprintf(hostbuf, sizeof(hostbuf), "%.*s", (int)(colon - hp), hp);
                if (colon[1]) { u->port = atoi(colon + 1); u->has_port = 1; }
            } else snprintf(hostbuf, sizeof(hostbuf), "%s", hp);
        }
    }
    for (i = 0; i < strlen(hostbuf); ++i) {
        unsigned char c = (unsigned char)hostbuf[i];
        u->host[i] = (char)(c >= 'A' && c <= 'Z' ? (c + 32) : c);
    }
    u->host[strlen(hostbuf)] = '\0';
    for (i = 0; i < strlen(u->scheme); ++i) {
        unsigned char c = (unsigned char)u->scheme[i];
        u->scheme[i] = (char)(c >= 'A' && c <= 'Z' ? (c + 32) : c);
    }
    free(main);
}

/* f"{scheme}://{host.lower()}:{port or default}{path.rstrip('/')}" */
static char *canonical_endpoint_str(const char *base_url) {
    Url41 u;
    char portbuf[32], out[4096];
    size_t pl;
    parse41(base_url ? base_url : "", &u);
    if (u.has_port) snprintf(portbuf, sizeof(portbuf), "%d", u.port);
    else if (!strcmp(u.scheme, "http")) snprintf(portbuf, sizeof(portbuf), "80");
    else if (!strcmp(u.scheme, "https")) snprintf(portbuf, sizeof(portbuf), "443");
    else portbuf[0] = '\0';
    pl = strlen(u.path);
    while (pl > 0 && u.path[pl-1] == '/') u.path[--pl] = '\0';
    snprintf(out, sizeof(out), "%s://%s:%s%s", u.scheme, u.host, portbuf, u.path);
    return dupstr(out);
}

json_t *w17_configsecr_80ebc4_canonical_endpoint(const char *base_url) {
    char *s = canonical_endpoint_str(base_url);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* host[_port][_path]_<digest(canonical)> */
static char *endpoint_identity_str(const char *base_url) {
    Url41 u;
    char *canon, *dig, *res;
    char path[2048];
    size_t pl;
    parse41(base_url ? base_url : "", &u);
    if (!u.host[0]) return dupstr("");
    snprintf(path, sizeof(path), "%s", u.path);
    pl = strlen(path);
    while (pl > 0 && path[pl-1] == '/') path[--pl] = '\0';
    {
        size_t st = 0;
        while (st < pl && path[st] == '/') st++;
        memmove(path, path + st, pl - st + 1);
    }
    canon = canonical_endpoint_str(base_url);
    dig = digest_str(canon ? canon : "");
    free(canon);
    res = (char *)malloc(strlen(u.host) + strlen(path) + strlen(dig ? dig : "") + 96);
    if (!res) { free(dig); return dupstr(""); }
    if (u.has_port) snprintf(res, strlen(u.host) + strlen(path) + strlen(dig ? dig : "") + 96,
                             "%s_%d", u.host, u.port);
    else snprintf(res, strlen(u.host) + strlen(path) + strlen(dig ? dig : "") + 96, "%s", u.host);
    pl = strlen(res);
    if (path[0]) {
        char low[2048];
        size_t i;
        for (i = 0; i < strlen(path); ++i) {
            unsigned char c = (unsigned char)path[i];
            low[i] = (char)(c >= 'A' && c <= 'Z' ? (c + 32) : c);
        }
        low[strlen(path)] = '\0';
        if (strcmp(low, "v1") != 0) snprintf(res + pl, 2048, "_%s", path);
    }
    pl = strlen(res);
    snprintf(res + pl, 64, "_%s", dig ? dig : "");
    free(dig);
    return res;
}

json_t *w17_configsecr_80ebc4_endpoint_identity(const char *base_url) {
    char *s = endpoint_identity_str(base_url);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* HERMES_CUSTOM_<slug>_API_KEY (config.custom_endpoint_key_env) */
static char *custom_endpoint_key_env(const char *identity) {
    char *slug = env_slug_str(identity);
    char *res = (char *)malloc(strlen(slug ? slug : "") + 40);
    if (!res) { free(slug); return dupstr(""); }
    if (slug && *slug) snprintf(res, strlen(slug) + 40, "HERMES_CUSTOM_%s_API_KEY", slug);
    else snprintf(res, 40, "HERMES_CUSTOM_API_KEY");
    free(slug);
    return res;
}

json_t *w17_configsecr_80ebc4_endpoint_api_key_env(const char *base_url) {
    char *id = endpoint_identity_str(base_url);
    char *res;
    json_t *r;
    if (!id || !*id) { free(id); return json_string("HERMES_MODEL_API_KEY"); }
    res = custom_endpoint_key_env(id);
    free(id);
    r = json_string(res ? res : "");
    free(res);
    return r;
}

json_t *w17_configsecr_80ebc4_dashboard_endpoint_key_env(const json_t *entry,
                                                         const char *endpoint_id) {
    const char *cur = json_get_str(entry, "key_env", "");
    json_t *r;
    if (!strncmp(cur, "HERMES_CUSTOM_", 14) || !strncmp(cur, "HERMES_PROVIDER_", 16))
        return json_string(cur);
    r = w17_configsecr_80ebc4_provider_name_key_env(endpoint_id ? endpoint_id : "");
    return r;
}

static char *strip_copy_dup(const char *s) {
    const char *a = s ? s : "", *b;
    char *d;
    size_t n;
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    d = (char *)malloc(n + 1);
    if (d) { memcpy(d, a, n); d[n] = '\0'; }
    return d;
}

json_t *w17_configsecr_80ebc4_block_base_url(const json_t *block) {
    static const char *const keys[] = {"base_url", "url", "api", NULL};
    int i;
    for (i = 0; keys[i]; ++i) {
        json_t *v = json_obj_get(block, keys[i]);
        const char *s = json_get_str(block, keys[i], "");
        if (v && v->type == JSON_STRING && s && *s) {
            char *t = strip_copy_dup(s);
            json_t *r = json_string(t ? t : "");
            free(t);
            return r;
        }
    }
    return json_string("");
}
