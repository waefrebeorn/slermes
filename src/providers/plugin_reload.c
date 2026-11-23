/*
 * plugin_reload.c — C home for upstream Hermes providers/plugin_reload.py.
 *
 * PoP: w9_pluginreloa_37afa1_tree_fingerprint @ providers/plugin_reload.py:tree_fingerprint
 *
 * The walk/stat layer is injected as a JSON array; the fingerprint algorithm
 * (filter, sort, join, sha1, 12-hex truncation) is ported verbatim.  A small
 * self-contained SHA-1 implementation is included so the module has no link
 * dependency beyond libjson.
 */
#include "hermes_json.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w9_pluginreloa_37afa1.h"

/* --- minimal SHA-1 ------------------------------------------------------ */
typedef struct {
    uint32_t h[5];
    uint64_t bits;
    unsigned char buf[64];
    size_t used;
} sha1_ctx;

static uint32_t rol32(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(sha1_ctx *c, const unsigned char *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
               ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
    for (int i = 16; i < 80; i++)
        w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & cc) | ((~b) & d); k = 0x5A827999u; }
        else if (i < 40) { f = b ^ cc ^ d; k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDCu; }
        else { f = b ^ cc ^ d; k = 0xCA62C1D6u; }
        uint32_t t = rol32(a, 5) + f + e + k + w[i];
        e = d; d = cc; cc = rol32(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}

static void sha1_init(sha1_ctx *c) {
    c->h[0] = 0x67452301u; c->h[1] = 0xEFCDAB89u; c->h[2] = 0x98BADCFEu;
    c->h[3] = 0x10325476u; c->h[4] = 0xC3D2E1F0u;
    c->bits = 0; c->used = 0;
}

static void sha1_update(sha1_ctx *c, const unsigned char *data, size_t n) {
    c->bits += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->used;
        if (take > n) take = n;
        memcpy(c->buf + c->used, data, take);
        c->used += take; data += take; n -= take;
        if (c->used == 64) { sha1_block(c, c->buf); c->used = 0; }
    }
}

static void sha1_final(sha1_ctx *c, unsigned char out[20]) {
    uint64_t bits = c->bits;
    unsigned char pad = 0x80;
    sha1_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->used != 56) sha1_update(c, &zero, 1);
    unsigned char tail[8];
    for (int i = 0; i < 8; i++) tail[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha1_update(c, tail, 8);
    for (int i = 0; i < 5; i++) {
        out[4 * i]     = (unsigned char)(c->h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(c->h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(c->h[i] >> 8);
        out[4 * i + 3] = (unsigned char)c->h[i];
    }
}

typedef struct {
    const char *name;
    const char *size;
    const char *mtime;
    size_t idx;
} fp_entry;

static int fp_cmp(const void *a, const void *b) {
    const fp_entry *x = (const fp_entry *)a, *y = (const fp_entry *)b;
    int c = strcmp(x->name, y->name);
    /* Python's sorted() is stable; qsort is not.  Break ties by input index. */
    return c ? c : (x->idx < y->idx ? -1 : (x->idx > y->idx ? 1 : 0));
}

struct json_t *w9_pluginreloa_37afa1_tree_fingerprint(const struct json_t *files) {
    fp_entry entries[512];
    size_t n = 0;
    if (files && files->type == JSON_ARRAY) {
        for (size_t i = 0; i < files->c.count && n < 512; i++) {
            json_t *f = files->c.items[i];
            if (!f || f->type != JSON_OBJECT) continue;
            const char *name = json_get_str(f, "name", "");
            int keep = (strlen(name) >= 3 && !strcmp(name + strlen(name) - 3, ".py")) ||
                       !strcmp(name, "plugin.yaml") || !strcmp(name, "plugin.yml");
            if (!keep) continue;
            entries[n].name = name;
            entries[n].size = json_get_str(f, "size", "0");
            entries[n].mtime = json_get_str(f, "mtime_ns", "0");
            entries[n].idx = n;
            n++;
        }
    }
    qsort(entries, n, sizeof(entries[0]), fp_cmp);

    char joined[65536];
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        int w = snprintf(joined + used, sizeof(joined) - used, "%s%s:%s:%s",
                         i ? "\n" : "", entries[i].name, entries[i].size, entries[i].mtime);
        if (w < 0 || (size_t)w >= sizeof(joined) - used) break;
        used += (size_t)w;
    }
    sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, (const unsigned char *)joined, used);
    unsigned char digest[20];
    sha1_final(&ctx, digest);
    char hex[41];
    for (int i = 0; i < 20; i++) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    hex[12] = '\0';
    return json_string(hex);
}

void providers_plugin_reload_port_pending(void) { (void)0; }
