/*
 * termux_libs.c — C home for upstream Hermes pm/termux_libs.py
 * Gate: w5_termuxlibs_3b3b3b
 *
 * PoP: w5_termuxlibs_3b3b3b_package_of @ pm/termux_libs.py:package_of
 * PoP: w5_termuxlibs_3b3b3b_filename @ pm/termux_libs.py:_filename
 * PoP: w5_termuxlibs_3b3b3b_parse_index @ pm/termux_libs.py:parse_index
 * PoP: w5_termuxlibs_3b3b3b_constants @ pm/termux_libs.py:<module>
 *
 * No network here: index()/repair() are I/O shells over the pure parsers below.
 */
#include "noc_w5_termuxlibs_3b3b3b.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TL_POOL "https://packages.termux.dev/apt/termux-main/"
#define TL_POOL_HOST "packages.termux.dev"
#define TL_INDEX_URL TL_POOL "dists/stable/main/binary-aarch64/Packages"
#define TL_POOL_PATH_PREFIX "/apt/termux-main/pool/main/"
#define TL_TABLE_NAME "termux_runtime_libs.json"

static char *tl_dup(const char *s) { return strdup(s ? s : ""); }

static char *tl_strip(const char *s) {
    const char *a = s ? s : "";
    while (*a && isspace((unsigned char)*a)) a++;
    size_t n = strlen(a);
    while (n && isspace((unsigned char)a[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, a, n); out[n] = '\0'; }
    return out;
}

char *w5_termuxlibs_3b3b3b_package_of(const char *url) {
    if (!url) return NULL;
    const char *sep = strstr(url, "://");
    if (!sep || sep == url || !isalpha((unsigned char)url[0])) return NULL;
    for (const char *q = url + 1; q < sep; q++) {
        unsigned char c = (unsigned char)*q;
        if (!isalnum(c) && c != '+' && c != '.' && c != '-') return NULL;
    }
    size_t sl = (size_t)(sep - url);
    char *scheme = (char *)malloc(sl + 1);
    if (!scheme) return NULL;
    memcpy(scheme, url, sl); scheme[sl] = '\0';
    for (size_t i = 0; i < sl; i++) scheme[i] = (char)tolower((unsigned char)scheme[i]);
    int https = (strcmp(scheme, "https") == 0);
    free(scheme);
    if (!https) return NULL;

    /* urlsplit().netloc is case-preserving and includes any userinfo/port. */
    const char *net = sep + 3;
    const char *netend = net;
    while (*netend && *netend != '/' && *netend != '?' && *netend != '#') netend++;
    size_t nlen = (size_t)(netend - net);
    if (strlen(TL_POOL_HOST) != nlen || memcmp(net, TL_POOL_HOST, nlen) != 0) return NULL;

    if (*netend != '/') return NULL;              /* urlsplit().path is empty */
    const char *path = netend;
    const char *pend = path;
    while (*pend && *pend != '?' && *pend != '#') pend++;
    size_t plen = (size_t)(pend - path);
    size_t prelen = strlen(TL_POOL_PATH_PREFIX);
    if (plen < prelen || memcmp(path, TL_POOL_PATH_PREFIX, prelen) != 0) return NULL;

    const char *rest = path + prelen;
    size_t rlen = plen - prelen;
    const char *seg[4]; size_t seglen[4]; size_t nseg = 0; int empty = 0;
    const char *s = rest, *e = rest + rlen;
    while (s <= e && nseg < 4) {
        const char *c = s;
        while (c < e && *c != '/') c++;
        seg[nseg] = s; seglen[nseg] = (size_t)(c - s);
        if (seglen[nseg] == 0) empty = 1;
        nseg++;
        if (c == e) break;
        s = c + 1;
    }
    if (nseg != 3 || empty) return NULL;
    char *out = (char *)malloc(seglen[1] + 1);
    if (!out) return NULL;
    memcpy(out, seg[1], seglen[1]); out[seglen[1]] = '\0';
    return out;
}

char *w5_termuxlibs_3b3b3b_filename(const char *url) {
    const char *u = url ? url : "";
    const char *slash = strrchr(u, '/');
    return tl_dup(slash ? slash + 1 : u);
}

/* ---- RFC822 field map -------------------------------------------------- */

typedef struct { char **keys, **vals; size_t n, cap; } TLFields;

static void tl_fset(TLFields *f, const char *k, const char *v) {
    for (size_t i = 0; i < f->n; i++) {
        if (!strcmp(f->keys[i], k)) { free(f->vals[i]); f->vals[i] = tl_dup(v); return; }
    }
    if (f->n == f->cap) {
        size_t nc = f->cap ? f->cap * 2 : 16;
        char **nk = (char **)realloc(f->keys, nc * sizeof(char *));
        char **nv = (char **)realloc(f->vals, nc * sizeof(char *));
        if (!nk || !nv) { free(nk); free(nv); return; }
        f->keys = nk; f->vals = nv; f->cap = nc;
    }
    f->keys[f->n] = tl_dup(k); f->vals[f->n] = tl_dup(v); f->n++;
}

static const char *tl_fget(TLFields *f, const char *k) {
    for (size_t i = 0; i < f->n; i++) if (!strcmp(f->keys[i], k)) return f->vals[i];
    return NULL;
}

static void tl_fclear(TLFields *f) {
    for (size_t i = 0; i < f->n; i++) { free(f->keys[i]); free(f->vals[i]); }
    f->n = 0;
}

static void tl_ffree(TLFields *f) {
    tl_fclear(f); free(f->keys); free(f->vals);
}

/* _package(fields): PoolPackage or NULL when not installable evidence. */
static json_t *tl_package(TLFields *f) {
    const char *name = tl_fget(f, "Package");
    const char *filename = tl_fget(f, "Filename");
    const char *sha = tl_fget(f, "SHA256");
    if (!name || !*name || !filename || !*filename || !sha || !*sha) return NULL;
    const char *version = tl_fget(f, "Version");
    json_t *row = json_object();
    json_set(row, "name", json_string(name));
    json_set(row, "version", json_string(version ? version : ""));
    size_t ul = strlen(TL_POOL) + strlen(filename);
    char *url = (char *)malloc(ul + 1);
    if (url) {
        memcpy(url, TL_POOL, strlen(TL_POOL));
        memcpy(url + strlen(TL_POOL), filename, strlen(filename));
        url[ul] = '\0';
        json_set(row, "url", json_string(url));
        free(url);
    }
    json_set(row, "sha256", json_string(sha));
    return row;
}

struct json_t *w5_termuxlibs_3b3b3b_parse_index(const char *text) {
    const char *src = text ? text : "";
    size_t len = strlen(src);
    char **lines = NULL; size_t nl = 0, cap = 0;
    size_t i = 0;
    while (i < len) {
        size_t start = i;
        while (i < len && src[i] != '\n' && src[i] != '\r') i++;
        size_t n = i - start;
        char *ln = (char *)malloc(n + 1);
        if (ln) {
            memcpy(ln, src + start, n); ln[n] = '\0';
            if (nl == cap) {
                size_t nc = cap ? cap * 2 : 32;
                char **tmp = (char **)realloc(lines, nc * sizeof(char *));
                if (tmp) { lines = tmp; cap = nc; }
            }
            if (nl < cap) lines[nl++] = ln; else free(ln);
        }
        if (i < len) {
            if (src[i] == '\r' && i + 1 < len && src[i + 1] == '\n') i += 2;
            else i++;
        }
    }
    /* Python: for line in [*text.splitlines(), ""] — always one terminating flush. */
    if (nl == cap) {
        size_t nc = cap ? cap * 2 : 32;
        char **tmp = (char **)realloc(lines, nc * sizeof(char *));
        if (tmp) { lines = tmp; cap = nc; }
    }
    if (nl < cap) lines[nl++] = strdup("");

    TLFields fields; memset(&fields, 0, sizeof(fields));
    json_t *pool = json_object();
    for (size_t li = 0; li < nl; li++) {
        char *line = lines[li];
        if (!line) continue;
        size_t llen = strlen(line);
        char *stripped = tl_strip(line);
        if (!stripped) continue;
        if (*stripped == '\0') {
            json_t *row = tl_package(&fields);
            if (row) {
                const char *nm = tl_fget(&fields, "Package");
                json_set(pool, nm ? nm : "", row);   /* json_set replaces: last row wins */
            }
            tl_fclear(&fields);
        } else if (!isspace((unsigned char)line[0])) {
            /* ^([A-Za-z][A-Za-z0-9-]*):[ ]?(.*)$ */
            if (isalpha((unsigned char)line[0])) {
                size_t k = 1;
                while (k < llen && (isalnum((unsigned char)line[k]) || line[k] == '-')) k++;
                if (k < llen && line[k] == ':') {
                    size_t vs = k + 1;
                    if (vs < llen && line[vs] == ' ') vs++;  /* [ ]? is ONE space */
                    char *val = tl_strip(line + vs);
                    char *key = (char *)malloc(k + 1);
                    if (key && val) {
                        memcpy(key, line, k); key[k] = '\0';
                        tl_fset(&fields, key, val);
                    }
                    free(key); free(val);
                }
            }
        }
        free(stripped);
        free(line);
    }
    free(lines);
    tl_ffree(&fields);
    return pool;
}

struct json_t *w5_termuxlibs_3b3b3b_constants(void) {
    json_t *o = json_object();
    json_set(o, "POOL", json_string(TL_POOL));
    json_set(o, "POOL_HOST", json_string(TL_POOL_HOST));
    json_set(o, "INDEX_URL", json_string(TL_INDEX_URL));
    json_set(o, "POOL_PATH_PREFIX", json_string(TL_POOL_PATH_PREFIX));
    json_set(o, "TABLE_NAME", json_string(TL_TABLE_NAME));
    return o;
}
