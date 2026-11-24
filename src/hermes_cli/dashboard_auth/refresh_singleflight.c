/*
 * refresh_singleflight.c -- C11 port of the replay-cache core of upstream Hermes
 *   hermes_cli/dashboard_auth/refresh_singleflight.py
 *
 * PoP: w21_refreshsin_e3dbfd_digest @ hermes_cli/dashboard_auth/refresh_singleflight.py:_digest
 * PoP: w21_refreshsin_e3dbfd_forget @ hermes_cli/dashboard_auth/refresh_singleflight.py:forget
 * PoP: w21_refreshsin_e3dbfd_prune @ hermes_cli/dashboard_auth/refresh_singleflight.py:_prune
 * PoP: w21_refreshsin_e3dbfd_refresh @ hermes_cli/dashboard_auth/refresh_singleflight.py:_refresh_provider
 * PoP: w21_refreshsin_e3dbfd_constants @ hermes_cli/dashboard_auth/refresh_singleflight.py:_SUCCESS_TTL
 *
 * refresh_session_coalesced() and the provider stack walk
 * (scan_session_providers) stay upstream: they are bound to live provider
 * objects, logging and a threadpool. Everything else below is the real work --
 * the cache is modeled as the ordered dict Python keeps, including the
 * insertion order of survivors and the first-seen tie break of min().
 */
#include "hermes_json.h"
#include "noc_w21_refreshsin_e3dbfd.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- sha256 (self-contained; the gate links against libjson alone) --------- */

typedef struct {
    uint32_t h[8];
    uint64_t total;
    unsigned char buf[64];
    size_t fill;
} refreshsin_sha256;

static const uint32_t REFRESHSIN_K[64] = {
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

static uint32_t refreshsin_rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static void refreshsin_sha256_init(refreshsin_sha256 *c) {
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u; c->h[2] = 0x3c6ef372u;
    c->h[3] = 0xa54ff53au; c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu;
    c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->total = 0;
    c->fill = 0;
}

static void refreshsin_sha256_block(refreshsin_sha256 *c, const unsigned char *p) {
    uint32_t w[64], a, b, cc, d, e, f, g, h, s0, s1, ch, maj, t1, t2;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        s0 = refreshsin_rotr(w[i - 15], 7) ^ refreshsin_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        s1 = refreshsin_rotr(w[i - 2], 17) ^ refreshsin_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3];
    e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (i = 0; i < 64; i++) {
        s1 = refreshsin_rotr(e, 6) ^ refreshsin_rotr(e, 11) ^ refreshsin_rotr(e, 25);
        ch = (e & f) ^ ((~e) & g);
        t1 = h + s1 + ch + REFRESHSIN_K[i] + w[i];
        s0 = refreshsin_rotr(a, 2) ^ refreshsin_rotr(a, 13) ^ refreshsin_rotr(a, 22);
        maj = (a & b) ^ (a & cc) ^ (b & cc);
        t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void refreshsin_sha256_update(refreshsin_sha256 *c, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    c->total += (uint64_t)len;
    while (len > 0) {
        size_t take = 64 - c->fill;
        if (take > len) take = len;
        memcpy(c->buf + c->fill, p, take);
        c->fill += take;
        p += take;
        len -= take;
        if (c->fill == 64) {
            refreshsin_sha256_block(c, c->buf);
            c->fill = 0;
        }
    }
}

static void refreshsin_sha256_final(refreshsin_sha256 *c, unsigned char out[32]) {
    uint64_t bits = c->total * 8u;
    unsigned char pad[72];
    size_t padlen, i;
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    padlen = (c->fill < 56) ? (56 - c->fill) : (120 - c->fill);
    refreshsin_sha256_update(c, pad, padlen);
    for (i = 0; i < 8; i++) pad[i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xffu);
    refreshsin_sha256_update(c, pad, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(c->h[i]);
    }
}

static char *refreshsin_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static void refreshsin_body_hex(const unsigned char body[32], char out[65]) {
    static const char HEX[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 32; i++) {
        out[i * 2] = HEX[body[i] >> 4];
        out[i * 2 + 1] = HEX[body[i] & 0x0f];
    }
    out[64] = '\0';
}

/* body = sha256(token) raw bytes; token NULL reads as "". */
static void refreshsin_body(const char *token, unsigned char body[32]) {
    refreshsin_sha256 ctx;
    refreshsin_sha256_init(&ctx);
    if (token && *token) refreshsin_sha256_update(&ctx, token, strlen(token));
    refreshsin_sha256_final(&ctx, body);
}

char *w21_refreshsin_e3dbfd_digest(const char *token) {
    unsigned char body[32];
    char hex[65];
    refreshsin_body(token, body);
    refreshsin_body_hex(body, hex);
    return refreshsin_dup(hex);
}

json_t *w21_refreshsin_e3dbfd_constants(void) {
    json_t *out = json_object();
    json_set(out, "success_ttl", json_number(W21_REFRESHSIN_E3DBFD_SUCCESS_TTL));
    json_set(out, "failure_ttl", json_number(W21_REFRESHSIN_E3DBFD_FAILURE_TTL));
    json_set(out, "max_entries", json_number(W21_REFRESHSIN_E3DBFD_MAX_ENTRIES));
    return out;
}

/* --- forget --------------------------------------------------------------- */

#define REFRESHSIN_ROWS 320
#define REFRESHSIN_FLIGHTS 128
#define REFRESHSIN_LABEL 320

typedef struct {
    int provider;
    char token[REFRESHSIN_LABEL];
    char digest[65];
    char issued[65];
    int has_issued;
    int alive;
} refreshsin_cache_row;

typedef struct {
    int provider;
    char token[REFRESHSIN_LABEL];
    char digest[65];
    long long generation;
} refreshsin_flight_row;

static int refreshsin_linked_has(char linked[][65], int n, const char *digest) {
    int i;
    for (i = 0; i < n; i++) {
        if (!strcmp(linked[i], digest)) return 1;
    }
    return 0;
}

static void refreshsin_linked_add(char linked[][65], int *n, int cap, const char *digest) {
    if (!digest || !digest[0]) return;
    if (refreshsin_linked_has(linked, *n, digest)) return;
    if (*n >= cap) return;
    memcpy(linked[*n], digest, 65);
    (*n)++;
}

static int refreshsin_cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

json_t *w21_refreshsin_e3dbfd_forget(const json_t *entries, const json_t *flights,
                                     const char *token) {
    static refreshsin_cache_row rows[REFRESHSIN_ROWS];
    static refreshsin_flight_row fly[REFRESHSIN_FLIGHTS];
    static char linked[REFRESHSIN_ROWS + REFRESHSIN_FLIGHTS + 1][65];
    static char labels[REFRESHSIN_ROWS][REFRESHSIN_LABEL + 32];
    int nrows = 0, nfly = 0, nlinked = 0, nlabels = 0;
    int changed = 1, i;
    size_t idx;
    json_t *out, *removed, *remaining, *generations;
    unsigned char body[32];

    memset(rows, 0, sizeof(rows));
    memset(fly, 0, sizeof(fly));
    memset(linked, 0, sizeof(linked));
    memset(labels, 0, sizeof(labels));

    if (entries && entries->type == JSON_ARRAY) {
        for (idx = 0; idx < entries->c.count && nrows < REFRESHSIN_ROWS; idx++) {
            json_t *e = entries->c.items[idx];
            json_t *issued;
            const char *tok;
            if (!e || e->type != JSON_OBJECT) continue;
            tok = json_get_str(e, "token", "");
            rows[nrows].provider = (int)json_get_num(e, "provider", 0.0);
            snprintf(rows[nrows].token, sizeof(rows[nrows].token), "%s", tok ? tok : "");
            refreshsin_body(rows[nrows].token, body);
            refreshsin_body_hex(body, rows[nrows].digest);
            issued = json_obj_get(e, "issued");
            rows[nrows].has_issued = 0;
            if (issued && issued->type == JSON_STRING && issued->str_val && issued->str_val[0]) {
                refreshsin_body(issued->str_val, body);
                refreshsin_body_hex(body, rows[nrows].issued);
                rows[nrows].has_issued = 1;
            }
            rows[nrows].alive = 1;
            nrows++;
        }
    }
    if (flights && flights->type == JSON_ARRAY) {
        for (idx = 0; idx < flights->c.count && nfly < REFRESHSIN_FLIGHTS; idx++) {
            json_t *f = flights->c.items[idx];
            const char *tok;
            if (!f || f->type != JSON_OBJECT) continue;
            tok = json_get_str(f, "token", "");
            fly[nfly].provider = (int)json_get_num(f, "provider", 0.0);
            snprintf(fly[nfly].token, sizeof(fly[nfly].token), "%s", tok ? tok : "");
            refreshsin_body(fly[nfly].token, body);
            refreshsin_body_hex(body, fly[nfly].digest);
            fly[nfly].generation = (long long)json_get_num(f, "generation", 0.0);
            nfly++;
        }
    }

    refreshsin_body(token, body);
    {
        char hex[65];
        refreshsin_body_hex(body, hex);
        refreshsin_linked_add(linked, &nlinked, REFRESHSIN_ROWS + REFRESHSIN_FLIGHTS, hex);
    }

    while (changed) {
        changed = 0;
        for (i = 0; i < nrows; i++) {
            if (!rows[i].alive) continue;
            if (refreshsin_linked_has(linked, nlinked, rows[i].digest) ||
                (rows[i].has_issued && refreshsin_linked_has(linked, nlinked, rows[i].issued))) {
                rows[i].alive = 0;
                refreshsin_linked_add(linked, &nlinked, REFRESHSIN_ROWS + REFRESHSIN_FLIGHTS,
                                      rows[i].digest);
                if (rows[i].has_issued) {
                    refreshsin_linked_add(linked, &nlinked,
                                          REFRESHSIN_ROWS + REFRESHSIN_FLIGHTS, rows[i].issued);
                }
                changed = 1;
            }
        }
    }

    removed = json_array();
    remaining = json_array();
    generations = json_array();
    nlabels = 0;
    for (i = 0; i < nrows; i++) {
        if (rows[i].alive) continue;
        snprintf(labels[nlabels], sizeof(labels[nlabels]), "%d:%s",
                 rows[i].provider, rows[i].token);
        nlabels++;
    }
    qsort(labels, (size_t)nlabels, sizeof(labels[0]), refreshsin_cmp_str);
    for (i = 0; i < nlabels; i++) json_append(removed, json_string(labels[i]));
    for (i = 0; i < nrows; i++) {
        char buf[REFRESHSIN_LABEL + 32];
        if (!rows[i].alive) continue;
        snprintf(buf, sizeof(buf), "%d:%s", rows[i].provider, rows[i].token);
        json_append(remaining, json_string(buf));
    }
    nlabels = 0;
    for (i = 0; i < nfly; i++) {
        long long gen = fly[i].generation;
        if (refreshsin_linked_has(linked, nlinked, fly[i].digest)) gen += 1;
        snprintf(labels[nlabels], sizeof(labels[nlabels]), "%d:%s:%lld",
                 fly[i].provider, fly[i].token, gen);
        nlabels++;
    }
    qsort(labels, (size_t)nlabels, sizeof(labels[0]), refreshsin_cmp_str);
    for (i = 0; i < nlabels; i++) json_append(generations, json_string(labels[i]));

    out = json_object();
    json_set(out, "removed", removed);
    json_set(out, "remaining", remaining);
    json_set(out, "generations", generations);
    return out;
}

/* --- _prune --------------------------------------------------------------- */

json_t *w21_refreshsin_e3dbfd_prune(const json_t *entries, double now) {
    static char tokens[REFRESHSIN_ROWS][REFRESHSIN_LABEL];
    static double expiry[REFRESHSIN_ROWS];
    static char alive[REFRESHSIN_ROWS];
    int n = 0, i, dropped = 0;
    size_t idx;
    json_t *out, *remaining;

    memset(tokens, 0, sizeof(tokens));
    memset(alive, 0, sizeof(alive));
    if (entries && entries->type == JSON_ARRAY) {
        for (idx = 0; idx < entries->c.count && n < REFRESHSIN_ROWS; idx++) {
            json_t *e = entries->c.items[idx];
            const char *tok;
            if (!e || e->type != JSON_OBJECT) continue;
            tok = json_get_str(e, "token", "");
            snprintf(tokens[n], sizeof(tokens[n]), "%s", tok ? tok : "");
            expiry[n] = json_get_num(e, "expires", 0.0);
            alive[n] = 1;
            n++;
        }
    }
    for (i = 0; i < n; i++) {
        if (expiry[i] <= now) {
            alive[i] = 0;
            dropped++;
        }
    }
    for (;;) {
        int count = 0, victim = -1;
        for (i = 0; i < n; i++) {
            if (!alive[i]) continue;
            count++;
            /* min(_cache, key=expiry): first-seen wins a tie, like dict order. */
            if (victim < 0 || expiry[i] < expiry[victim]) victim = i;
        }
        if (count <= W21_REFRESHSIN_E3DBFD_MAX_ENTRIES || victim < 0) break;
        alive[victim] = 0;
        dropped++;
    }
    remaining = json_array();
    for (i = 0; i < n; i++) {
        if (alive[i]) json_append(remaining, json_string(tokens[i]));
    }
    out = json_object();
    json_set(out, "remaining", remaining);
    json_set(out, "dropped", json_number((double)dropped));
    return out;
}

/* --- _refresh_provider (cache half) --------------------------------------- */

static int refreshsin_is_session(const json_t *v) {
    return v && v->type == JSON_OBJECT;
}

json_t *w21_refreshsin_e3dbfd_refresh(const json_t *req) {
    json_t *cached = json_obj_get(req, "cached");
    json_t *session = NULL;
    json_t *out;
    int reuse = json_get_bool(req, "reuse_detection", 0);
    int expired = json_get_bool(req, "expired", 0);
    int has_session, hit = 0, wrote = 0;
    double ttl = -1.0;

    /* with _guard: cached = _cache.get(key); if cached[0] > now: return cached[2] */
    if (cached && cached->type == JSON_OBJECT && json_get_num(cached, "expires_in", 0.0) > 0.0) {
        hit = 1;
        session = json_obj_get(cached, "session");
    }

    has_session = refreshsin_is_session(session);
    if (!hit) {
        if (expired) {
            session = NULL;             /* except RefreshExpiredError: session = None */
        } else {
            session = json_obj_get(req, "session");
        }
        has_session = refreshsin_is_session(session);
        /* generation == flight.generation and (session is None or reuse_detection) */
        if (!has_session || reuse) {
            wrote = 1;
            ttl = has_session ? W21_REFRESHSIN_E3DBFD_SUCCESS_TTL
                              : W21_REFRESHSIN_E3DBFD_FAILURE_TTL;
        }
    } else {
        wrote = 1;                      /* the cache already holds this entry */
        ttl = json_get_num(cached, "expires_in", 0.0);
    }

    out = json_object();
    json_set(out, "session", has_session ? json_copy(session) : json_null());
    json_set(out, "hit", json_bool(hit));
    json_set(out, "cached", json_bool(wrote));
    if (ttl < 0.0) json_set(out, "ttl", json_null());
    else json_set(out, "ttl", json_number(ttl));
    return out;
}
