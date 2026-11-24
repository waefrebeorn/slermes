/*
 * _cache.c - C11 port of upstream agent/secret_sources/_cache.py
 * (gate w26_cache_8bdf62).
 *
 * PoP: w26_cache_8bdf62_fingerprint @ agent/secret_sources/_cache.py:fingerprint
 * PoP: w26_cache_8bdf62_is_fresh @ agent/secret_sources/_cache.py:CachedFetch.is_fresh
 * PoP: w26_cache_8bdf62_entry_from_payload @ agent/secret_sources/_cache.py:entry_from_payload
 * PoP: w26_cache_8bdf62_disk_read @ agent/secret_sources/_cache.py:DiskCache.read
 * PoP: w26_cache_8bdf62_write_payload @ agent/secret_sources/_cache.py:DiskCache.write
 * PoP: w26_cache_8bdf62_lookup @ agent/secret_sources/_cache.py:SecretCache.lookup
 *
 * What is ported is the whole decision surface of the two-layer cache: the
 * SHA-256 key fingerprint, the TTL freshness test, the payload -> entry reader
 * that keeps only str -> str pairs, and the three decisions the layers make --
 * whether a read hits, whether a write happens and exactly what body it would
 * carry, and which layer answers a lookup.  The file open, the atomic write,
 * the clock and the home directory are seams and stay with the caller: the
 * fixture supplies what they would have returned.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_cache_8bdf62.h"

/* --- SHA-256 (FIPS 180-4) -----------------------------------------------
 * Required by fingerprint(); hashlib is not in C, so the one primitive the key
 * shape depends on is spelled out here. */

static const uint32_t CC_K[64] = {
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
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static uint32_t cc_ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void cc_sha256_block(uint32_t h[8], const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, k, t1, t2, s0, s1, ch, maj;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        s0 = cc_ror(w[i - 15], 7) ^ cc_ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        s1 = cc_ror(w[i - 2], 17) ^ cc_ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; k = h[7];
    for (i = 0; i < 64; i++) {
        s1 = cc_ror(e, 6) ^ cc_ror(e, 11) ^ cc_ror(e, 25);
        ch = (e & f) ^ (~e & g);
        t1 = k + s1 + ch + CC_K[i] + w[i];
        s0 = cc_ror(a, 2) ^ cc_ror(a, 13) ^ cc_ror(a, 22);
        maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = s0 + maj;
        k = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}

/* hexdigest()[:16] of data.  Caller frees. */
static char *cc_sha256_prefix16(const unsigned char *data, size_t len) {
    uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                     0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    unsigned char block[64];
    unsigned char *padded;
    size_t total, i, chunks;
    char *out;
    static const char *hex = "0123456789abcdef";

    total = ((len + 9 + 63) / 64) * 64;
    padded = (unsigned char *)calloc(total, 1);
    if (!padded) return NULL;
    memcpy(padded, data, len);
    padded[len] = 0x80;
    {
        uint64_t bits = (uint64_t)len * 8u;
        for (i = 0; i < 8; i++)
            padded[total - 1 - i] = (unsigned char)((bits >> (8 * i)) & 0xffu);
    }
    chunks = total / 64;
    for (i = 0; i < chunks; i++) {
        memcpy(block, padded + i * 64, 64);
        cc_sha256_block(h, block);
    }
    free(padded);

    out = (char *)malloc(17);
    if (!out) return NULL;
    for (i = 0; i < 8; i++) {
        unsigned char byte = (unsigned char)((h[i / 4] >> (24 - 8 * (i % 4))) & 0xffu);
        out[i * 2] = hex[byte >> 4];
        out[i * 2 + 1] = hex[byte & 0x0fu];
    }
    out[16] = '\0';
    return out;
}

/* --- shared value helpers ----------------------------------------------- */

static int cc_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cc_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }

/* Python numeric truthiness; a bool is an int. */
static int cc_num(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    return 0;
}

/* CachedFetch.is_fresh: ttl > 0 and (now - fetched_at) < ttl. */
static int cc_fresh(const json_t *fetched_at, const json_t *ttl, const json_t *now) {
    double f, t, n;
    if (!cc_num(ttl, &t)) return 0;
    if (!(t > 0.0)) return 0;
    if (!cc_num(fetched_at, &f) || !cc_num(now, &n)) return 0;
    return (n - f) < t;
}

/* entry_from_payload: {"secrets": {...}, "fetched_at": n} or NULL. */
static json_t *cc_entry(const json_t *payload) {
    const json_t *secrets, *fetched;
    json_t *out, *typed;
    size_t i, n;

    if (!cc_is_obj(payload)) return NULL;
    secrets = json_obj_get(payload, "secrets");
    fetched = json_obj_get(payload, "fetched_at");
    if (!cc_is_obj(secrets) || !fetched) return NULL;
    if (fetched->type != JSON_NUMBER && fetched->type != JSON_BOOL) return NULL;

    typed = json_object();
    n = json_object_size(secrets);
    for (i = 0; i < n; i++) {
        const json_t *v = json_object_get_at(secrets, i);
        const char *k = json_object_get_key_at(secrets, i);
        if (v && v->type == JSON_STRING && k) json_set(typed, k, json_copy(v));
    }
    out = json_object();
    json_set(out, "secrets", typed);
    json_set(out, "fetched_at",
             json_number(fetched->type == JSON_BOOL ? (fetched->bool_val ? 1.0 : 0.0)
                                                    : fetched->num_val));
    return out;
}

json_t *w26_cache_8bdf62_fingerprint(const json_t *req) {
    const json_t *material = json_obj_get(req, "material");
    char *hex;
    json_t *out;

    /* "x".encode("utf-8") -- a non-str has no .encode (AttributeError). */
    if (!cc_is_str(material)) return json_null();
    hex = cc_sha256_prefix16((const unsigned char *)(material->str_val ? material->str_val : ""),
                             strlen(material->str_val ? material->str_val : ""));
    if (!hex) return json_null();
    out = json_string(hex);
    free(hex);
    return out;
}

json_t *w26_cache_8bdf62_is_fresh(const json_t *req) {
    return json_bool(cc_fresh(json_obj_get(req, "fetched_at"), json_obj_get(req, "ttl_seconds"),
                              json_obj_get(req, "now")));
}

json_t *w26_cache_8bdf62_entry_from_payload(const json_t *req) {
    json_t *entry = cc_entry(json_obj_get(req, "payload"));
    return entry ? entry : json_null();
}

json_t *w26_cache_8bdf62_disk_read(const json_t *req) {
    const json_t *payload = json_obj_get(req, "payload");
    const json_t *want = json_obj_get(req, "want_key");
    const json_t *key;
    double ttl;
    json_t *entry;

    /* read() short-circuits before any file work when the TTL is off. */
    if (!cc_num(json_obj_get(req, "ttl_seconds"), &ttl) || !(ttl > 0.0)) return json_null();
    if (!cc_is_obj(payload)) return json_null();

    /* payload.get("key") != self._key_serializer(key), and a serializer is a
     * str function: a str never compares equal to any other JSON type. */
    key = json_obj_get(payload, "key");
    if (!cc_is_str(key) || !cc_is_str(want) ||
        strcmp(key->str_val ? key->str_val : "", want->str_val ? want->str_val : "") != 0)
        return json_null();

    entry = cc_entry(payload);
    if (!entry) return json_null();
    if (!cc_fresh(json_obj_get(entry, "fetched_at"), json_obj_get(req, "ttl_seconds"),
                  json_obj_get(req, "now"))) {
        json_free(entry);
        return json_null();
    }
    return entry;
}

json_t *w26_cache_8bdf62_write_payload(const json_t *req) {
    const json_t *entry = json_obj_get(req, "entry");
    const json_t *key = json_obj_get(req, "key");
    double ttl;
    json_t *out, *payload;

    out = json_object();
    /* write() returns before touching the disk when the TTL is off: an
     * opted-out user never gets secret values written at all. */
    if (!cc_num(json_obj_get(req, "ttl_seconds"), &ttl) || !(ttl > 0.0)) {
        json_set(out, "written", json_bool(0));
        json_set(out, "payload", json_null());
        return out;
    }
    payload = json_object();
    json_set(payload, "key", key ? json_copy(key) : json_string(""));
    json_set(payload, "secrets", json_copy(json_obj_get(entry, "secrets")));
    json_set(payload, "fetched_at", json_copy(json_obj_get(entry, "fetched_at")));
    json_set(out, "written", json_bool(1));
    json_set(out, "payload", payload);
    return out;
}

json_t *w26_cache_8bdf62_lookup(const json_t *req) {
    const json_t *memory = json_obj_get(req, "memory");
    const json_t *key = json_obj_get(req, "key");
    const json_t *disk = json_obj_get(req, "disk");
    const json_t *ttl = json_obj_get(req, "ttl_seconds");
    const json_t *now = json_obj_get(req, "now");
    json_t *out;

    out = json_object();
    if (cc_is_obj(memory) && cc_is_str(key)) {
        const json_t *mem = json_obj_get(memory, key->str_val ? key->str_val : "");
        /* L1 first: a fresh in-process entry never reaches the disk. */
        if (cc_is_obj(mem) && cc_fresh(json_obj_get(mem, "fetched_at"), ttl, now)) {
            json_set(out, "source", json_string("memory"));
            json_set(out, "entry", json_copy(mem));
            return out;
        }
    }
    if (cc_is_obj(disk)) {
        /* L2 answered; SecretCache.lookup promotes it into L1. */
        json_set(out, "source", json_string("disk"));
        json_set(out, "entry", json_copy(disk));
        return out;
    }
    json_set(out, "source", json_null());
    json_set(out, "entry", json_null());
    return out;
}
