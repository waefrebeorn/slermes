/*
 * tools_cache.c -- real C11 port of tools/connectors/portal/tools_cache.py.
 *
 * PoP: w22_toolscache_8cdd07_member_key @ tools/connectors/portal/tools_cache.py:_member_key
 * PoP: w22_toolscache_8cdd07_slug_valid @ tools/connectors/portal/client.py:validate_slug
 * PoP: w22_toolscache_8cdd07_cache_rel @ tools/connectors/portal/tools_cache.py:_cache_path
 * PoP: w22_toolscache_8cdd07_home_env @ tools/connectors/portal/tools_cache.py:<module>
 * PoP: w22_toolscache_8cdd07_read_plan @ tools/connectors/portal/tools_cache.py:read_tools
 *
 * Network client, cache directory and clock are seams. The JWT member key,
 * the slug grammar, the cache layout and the TTL/not-modified/stale lattice
 * are pure. SHA-256 is implemented here so the port stays dependency-free.
 */
#include "hermes_json.h"
#include "noc_w22_toolscache_8cdd07.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TC08013_TTL_SECONDS 86400.0

/* ---- SHA-256 ---------------------------------------------------------- */

typedef struct {
    uint32_t h[8];
    unsigned char block[64];
    size_t fill;
    uint64_t total;
} tc08013_sha;

static const uint32_t TC08013_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t tc08013_rotr(uint32_t v, int n) {
    return (v >> n) | (v << (32 - n));
}

static void tc08013_sha_block(tc08013_sha *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = tc08013_rotr(w[i - 15], 7) ^ tc08013_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = tc08013_rotr(w[i - 2], 17) ^ tc08013_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = tc08013_rotr(e, 6) ^ tc08013_rotr(e, 11) ^ tc08013_rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t S0 = tc08013_rotr(a, 2) ^ tc08013_rotr(a, 13) ^ tc08013_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + S1 + ch + TC08013_K[i] + w[i];
        t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void tc08013_sha_init(tc08013_sha *s) {
    s->h[0] = 0x6a09e667u; s->h[1] = 0xbb67ae85u; s->h[2] = 0x3c6ef372u;
    s->h[3] = 0xa54ff53au; s->h[4] = 0x510e527fu; s->h[5] = 0x9b05688cu;
    s->h[6] = 0x1f83d9abu; s->h[7] = 0x5be0cd19u;
    s->fill = 0;
    s->total = 0;
}

static void tc08013_sha_update(tc08013_sha *s, const unsigned char *data, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        s->block[s->fill++] = data[i];
        if (s->fill == 64) {
            tc08013_sha_block(s, s->block);
            s->total += 64;
            s->fill = 0;
        }
    }
}

static void tc08013_sha_final(tc08013_sha *s, unsigned char out[32]) {
    unsigned char pad[72];
    size_t padlen;
    uint64_t bits = (s->total + s->fill) * 8;
    int i;
    padlen = (s->fill < 56) ? (56 - s->fill) : (120 - s->fill);
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    for (i = 0; i < 8; i++) pad[padlen + i] = (unsigned char)(bits >> (56 - 8 * i));
    tc08013_sha_update(s, pad, padlen + 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(s->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(s->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(s->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)s->h[i];
    }
}

static void tc08013_sha_hex(const char *text, char out[65]) {
    static const char hexd[] = "0123456789abcdef";
    tc08013_sha s;
    unsigned char digest[32];
    int i;
    tc08013_sha_init(&s);
    tc08013_sha_update(&s, (const unsigned char *)(text ? text : ""), strlen(text ? text : ""));
    tc08013_sha_final(&s, digest);
    for (i = 0; i < 32; i++) {
        out[i * 2] = hexd[digest[i] >> 4];
        out[i * 2 + 1] = hexd[digest[i] & 0x0f];
    }
    out[64] = '\0';
}

static char *tc08013_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *tc08013_printf(const char *fmt, const char *a, const char *b, const char *c) {
    int need = snprintf(NULL, 0, fmt, a, b, c);
    char *out;
    if (need < 0) return NULL;
    out = (char *)malloc((size_t)need + 1);
    if (!out) return NULL;
    snprintf(out, (size_t)need + 1, fmt, a, b, c);
    return out;
}

/* ---- base64url -------------------------------------------------------- */

static int tc08013_b64val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* base64.urlsafe_b64decode: '-'/'_' mapped, other junk discarded, stop at '='. */
static unsigned char *tc08013_b64url(const char *text, size_t *outlen) {
    unsigned char *out;
    size_t cap, n = 0, gn = 0, i;
    unsigned int group[4];
    if (!text) return NULL;
    cap = strlen(text) / 4 * 3 + 3;
    out = (unsigned char *)malloc(cap);
    if (!out) return NULL;
    for (i = 0; text[i]; i++) {
        int v;
        if (text[i] == '=') break;
        v = tc08013_b64val((unsigned char)text[i]);
        if (v < 0) continue;
        group[gn++] = (unsigned int)v;
        if (gn == 4) {
            out[n++] = (unsigned char)((group[0] << 2) | (group[1] >> 4));
            out[n++] = (unsigned char)(((group[1] & 0x0f) << 4) | (group[2] >> 2));
            out[n++] = (unsigned char)(((group[2] & 0x03) << 6) | group[3]);
            gn = 0;
        }
    }
    if (gn == 1) {
        free(out);
        return NULL; /* binascii.Error: one leftover sextet */
    }
    if (gn == 2) {
        out[n++] = (unsigned char)((group[0] << 2) | (group[1] >> 4));
    } else if (gn == 3) {
        out[n++] = (unsigned char)((group[0] << 2) | (group[1] >> 4));
        out[n++] = (unsigned char)(((group[1] & 0x0f) << 4) | (group[2] >> 2));
    }
    out[n] = '\0';
    *outlen = n;
    return out;
}

char *w22_toolscache_8cdd07_member_key(const char *token) {
    const char *first, *second;
    char *payload, *decoded, *key;
    size_t plen = 0, dlen = 0;
    json_t *claims;
    const char *sub;
    char hex[65];
    if (!token) return NULL;
    first = strchr(token, '.');
    second = first ? strchr(first + 1, '.') : NULL;
    if (!first || !second) return NULL;
    if (strchr(second + 1, '.')) return NULL;
    plen = (size_t)(second - first - 1);
    payload = (char *)malloc(plen + 4);
    if (!payload) return NULL;
    memcpy(payload, first + 1, plen);
    payload[plen] = '\0';
    /* parts[1] + "=" * (-len(parts[1]) % 4) */
    {
        size_t pad = (4 - (plen % 4)) % 4;
        memset(payload + plen, '=', pad);
        payload[plen + pad] = '\0';
    }
    decoded = (char *)tc08013_b64url(payload, &dlen);
    free(payload);
    if (!decoded) return NULL;
    (void)dlen;
    claims = json_parse(decoded, NULL);
    free(decoded);
    if (!claims) return NULL;
    sub = NULL;
    if (claims->type == JSON_OBJECT) {
        json_t *node = json_obj_get(claims, "sub");
        if (node && node->type == JSON_STRING && node->str_val && node->str_val[0]) sub = node->str_val;
    }
    if (!sub) {
        json_free(claims);
        return NULL;
    }
    tc08013_sha_hex(sub, hex);
    hex[16] = '\0';
    key = tc08013_dup(hex);
    json_free(claims);
    return key;
}

int w22_toolscache_8cdd07_slug_valid(const char *slug) {
    const unsigned char *p = (const unsigned char *)(slug ? slug : "");
    if (!*p) return 0;
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) return 0;
    for (p++; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
            return 0;
    }
    return 1;
}

char *w22_toolscache_8cdd07_cache_rel(const char *origin, const char *member_key,
                                      const char *slug) {
    char hex[65];
    if (!member_key) return NULL;
    tc08013_sha_hex(origin ? origin : "", hex);
    hex[16] = '\0';
    return tc08013_printf("cache/connectors/%s/%s/%s.json", hex, member_key,
                          slug ? slug : "");
}

char *w22_toolscache_8cdd07_home_env(void) {
    const char *v = getenv("SLERMES_HOME");
    if (v && *v) return tc08013_dup(v);
    v = getenv("HERMES_HOME");
    if (v && *v) return tc08013_dup(v);
    v = getenv("HOME");
    if (v && *v) return tc08013_dup(v);
    return NULL;
}

/* ---- read_tools decision lattice -------------------------------------- */

static int tc08013_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}

/* The file seam: a cache entry exists only when the stored payload validates. */
static int tc08013_cache_present(const json_t *cached) {
    if (!cached || cached->type != JSON_OBJECT) return 0;
    if (!json_obj_get(cached, "connector")) return 0;
    if (!json_obj_get(cached, "toolkit_version")) return 0;
    if (!json_obj_get(cached, "etag")) return 0;
    if (!json_obj_get(cached, "fetched_at")) return 0;
    if (!json_obj_get(cached, "tools")) return 0;
    return 1;
}

static json_t *tc08013_out(const char *source, int stale, int has_ts, double ts,
                           int stored, int unlinked, const char *error) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "source", json_string(source));
    json_set(o, "stale", json_bool(stale ? 1 : 0));
    json_set(o, "fetched_at", has_ts ? json_number(ts) : json_null());
    json_set(o, "stored", json_bool(stored ? 1 : 0));
    json_set(o, "unlinked", json_bool(unlinked ? 1 : 0));
    json_set(o, "error", error ? json_string(error) : json_null());
    return o;
}

static const char *tc08013_listing_kind(const json_t *req) {
    const char *kind = json_get_str(req, "listing", "ok");
    return (kind && *kind) ? kind : "ok";
}

json_t *w22_toolscache_8cdd07_read_plan(const json_t *req) {
    const char *slug;
    const char *kind;
    char *member;
    int has_cached, refresh;
    double now, cached_ts = 0.0;
    json_t *cached;
    int member_is_mine;
    if (!req || req->type != JSON_OBJECT) return NULL;
    slug = json_get_str(req, "slug", "");
    if (!w22_toolscache_8cdd07_slug_valid(slug)) {
        return tc08013_out("raise", 0, 0, 0.0, 0, 0, "InvalidConnectorSlug");
    }
    member = w22_toolscache_8cdd07_member_key(json_get_str(req, "token", NULL));
    member_is_mine = member != NULL;
    kind = tc08013_listing_kind(req);
    now = json_get_num(req, "now", 0.0);
    refresh = tc08013_truthy(json_obj_get(req, "refresh"));
    cached = json_obj_get(req, "cached");
    has_cached = tc08013_cache_present(cached);
    if (has_cached) cached_ts = json_get_num(cached, "fetched_at", 0.0);

    if (!member_is_mine) {
        if (!strcmp(kind, "not_modified")) {
            free(member);
            return tc08013_out("raise", 0, 0, 0.0, 0, 0, "PortalToolsUnavailable");
        }
        free(member);
        if (!strcmp(kind, "ok")) return tc08013_out("network", 0, 1, now, 0, 0, NULL);
        if (!strcmp(kind, "gateway_unavailable")) return tc08013_out("raise", 0, 0, 0.0, 0, 0, "GatewayUnavailable");
        if (!strcmp(kind, "gateway_auth")) return tc08013_out("raise", 0, 0, 0.0, 0, 0, "GatewayAuthError");
        if (!strcmp(kind, "portal_unavailable")) return tc08013_out("raise", 0, 0, 0.0, 0, 0, "PortalToolsUnavailable");
        if (!strcmp(kind, "tool_gateway_error")) return tc08013_out("raise", 0, 0, 0.0, 0, 0, "ToolGatewayError");
        return tc08013_out("network", 0, 1, now, 0, 0, NULL);
    }

    {
        double age = now - cached_ts;
        if (has_cached && !refresh && age >= 0.0 && age < TC08013_TTL_SECONDS) {
            free(member);
            return tc08013_out("cache", 0, 1, cached_ts, 0, 0, NULL);
        }
    }

    if (!strcmp(kind, "ok")) {
        free(member);
        return tc08013_out("network", 0, 1, now, 1, 0, NULL);
    }
    if (!strcmp(kind, "not_modified")) {
        free(member);
        if (!has_cached) return tc08013_out("raise", 0, 0, 0.0, 0, 0, "PortalToolsUnavailable");
        return tc08013_out("revalidated", 0, 1, now, 1, 0, NULL);
    }
    if (!strcmp(kind, "gateway_unavailable")) {
        free(member);
        return tc08013_out("raise", 0, 0, 0.0, 0, 1, "GatewayUnavailable");
    }
    if (!strcmp(kind, "gateway_auth")) {
        free(member);
        return tc08013_out("raise", 0, 0, 0.0, 0, 0, "GatewayAuthError");
    }
    if (!strcmp(kind, "portal_unavailable") || !strcmp(kind, "tool_gateway_error")) {
        free(member);
        if (!has_cached) {
            return tc08013_out("raise", 0, 0, 0.0, 0, 0,
                               !strcmp(kind, "portal_unavailable") ? "PortalToolsUnavailable"
                                                                   : "ToolGatewayError");
        }
        return tc08013_out("cache", 1, 1, cached_ts, 0, 0, NULL);
    }
    free(member);
    return tc08013_out("network", 0, 1, now, 1, 0, NULL);
}
