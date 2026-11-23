/*
 * __init__.c — real C11 port of upstream Hermes providers/__init__.py.
 *
 * PoP: init_host_claims @ providers/__init__.py:_host_claims
 * PoP: init_module_name @ providers/__init__.py:_user_module_name
 * PoP: init_stamps_settled @ providers/__init__.py:_plugin_dir_stamps_settled
 *
 * Only the pure string/hash/timing helpers of the registry module are ported;
 * plugin discovery, import isolation and registry state are runtime concerns
 * and are not exercised by this differential gate.
 */
#include "noc_w6_init_cd8903.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MTIME_TICK_NS 2000000000LL

static char *trim_lower_rstrip_dots(const char *s) {
    const char *a = s ? s : "";
    const char *end;
    size_t n;
    char *out;
    while (*a && isspace((unsigned char)*a)) a++;
    end = a + strlen(a);
    while (end > a && isspace((unsigned char)end[-1])) end--;
    while (end > a && end[-1] == '.') end--;
    n = (size_t)(end - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)a[i]);
    out[n] = '\0';
    return out;
}

json_t *init_host_claims(const char *host, const char *claimed) {
    char *c = trim_lower_rstrip_dots(claimed);
    int result = 0;
    if (c && *c) {
        const char *h = host ? host : "";
        size_t cl = strlen(c), hl = strlen(h);
        if (!strcmp(h, c)) result = 1;
        else if (hl > cl && h[hl - cl - 1] == '.' && !strcmp(h + hl - cl, c)) result = 1;
    }
    free(c);
    return json_bool(result ? true : false);
}

/* ---------- minimal SHA-1 ---------- */
static uint32_t rol32(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1_digest(const unsigned char *data, size_t len, unsigned char out[20]) {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    size_t total = ((len + 9 + 63) / 64) * 64;
    unsigned char *m = (unsigned char *)calloc(1, total ? total : 64);
    uint64_t bits;
    if (!m) { memset(out, 0, 20); return; }
    memcpy(m, data, len);
    m[len] = 0x80;
    bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) m[total - 1 - i] = (unsigned char)(bits >> (8 * i));
    for (size_t off = 0; off < total; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)m[off + 4*i] << 24) | ((uint32_t)m[off + 4*i + 1] << 16) |
                   ((uint32_t)m[off + 4*i + 2] << 8) | (uint32_t)m[off + 4*i + 3];
        for (int i = 16; i < 80; i++)
            w[i] = rol32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k, t;
            if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
            t = rol32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol32(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    for (int i = 0; i < 5; i++) {
        out[4*i]     = (unsigned char)(h[i] >> 24);
        out[4*i + 1] = (unsigned char)(h[i] >> 16);
        out[4*i + 2] = (unsigned char)(h[i] >> 8);
        out[4*i + 3] = (unsigned char)h[i];
    }
    free(m);
}

static char *sha1_hex10(const char *s) {
    unsigned char dig[20];
    static const char hx[] = "0123456789abcdef";
    char *out = (char *)malloc(11);
    sha1_digest((const unsigned char *)(s ? s : ""), strlen(s ? s : ""), dig);
    if (!out) return NULL;
    for (int i = 0; i < 10; i++) {
        unsigned char b = dig[i / 2];
        out[i] = hx[(i % 2) ? (b & 15) : (b >> 4)];
    }
    out[10] = '\0';
    return out;
}

json_t *init_module_name(const char *name, const char *home_key) {
    char *digest = sha1_hex10(home_key);
    size_t n = strlen(name ? name : "");
    char *buf = (char *)malloc(64 + n * 2 + 4);
    json_t *r;
    if (!digest || !buf) { free(digest); free(buf); return json_string(""); }
    sprintf(buf, "_hermes_user_provider_%s_", digest);
    for (size_t i = 0; i < n; i++) {
        size_t k = strlen(buf);
        buf[k] = (name[i] == '-') ? '_' : name[i];
        buf[k + 1] = '\0';
    }
    r = json_string(buf);
    free(digest);
    free(buf);
    return r;
}

json_t *init_stamps_settled(const json_t *facts) {
    json_t *now_node, *stamps;
    long long now;
    int ok = 1;
    size_t i;
    if (!facts || facts->type != JSON_OBJECT) return json_bool(false);
    now_node = json_obj_get(facts, "now");
    stamps = json_obj_get(facts, "stamps");
    if (!now_node || now_node->type != JSON_STRING) return json_bool(false);
    now = strtoll(now_node->str_val, NULL, 10);
    if (!stamps || stamps->type != JSON_ARRAY) return json_bool(false);
    for (i = 0; i < json_len(stamps); i++) {
        json_t *s = json_get(stamps, i);
        long long v, diff;
        if (!s || s->type == JSON_NULL) continue;
        if (s->type != JSON_STRING) return json_bool(false);
        v = strtoll(s->str_val, NULL, 10);
        diff = v > now ? v - now : now - v;
        if (diff < MTIME_TICK_NS) { ok = 0; break; }
    }
    return json_bool(ok ? true : false);
}
