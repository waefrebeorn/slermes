/*
 * _runtime_retirement.c - C11 port of upstream
 * persistence/sqlite/authority/_runtime_retirement.py (gate w24_runtimeret_4ec6b1).
 *
 * PoP: w24_runtimeret_4ec6b1_identity_key @ persistence/sqlite/authority/_runtime_retirement.py:has_mutation_receipt -> _runtime.identity_key
 * PoP: w24_runtimeret_4ec6b1_receipt_key @ persistence/sqlite/authority/_runtime_retirement.py:has_mutation_receipt -> runtime.mutation_receipt_key
 * PoP: w24_runtimeret_4ec6b1_first_generation @ persistence/sqlite/authority/_runtime_retirement.py:first_generation
 * PoP: w24_runtimeret_4ec6b1_keep_passed_fences @ persistence/sqlite/authority/_runtime_retirement.py:keep_passed_fences
 * PoP: w24_runtimeret_4ec6b1_constants @ persistence/sqlite/authority/_runtime_retirement.py:<durable key prefixes>
 *
 * What is ported is the part that is a pure function of values: the durable
 * identity key a retried request is recognised by, the receipt key + digest the
 * exact-retry lookup compares (SHA-256 over the canonical admission
 * fingerprint, keys sorted, no separators -- byte-identical to the source's
 * json.dumps), the generation walk that skips retired ids, and the verdict a
 * bind records about the fences its walk passed.  The reads (state_meta,
 * sessions, the retired-id test) are seams: their rows arrive as arguments.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_runtimeret_4ec6b1.h"

#define RT_IDENTITY_PREFIX "gateway.terminal_identity.v1."
#define RT_MUTATION_PREFIX "gateway.mutation.v1."
#define RT_RETIRED_PREFIX "gateway.retired_session.v1."
#define RT_ADMISSION_PREFIX "gateway.terminal_admission.v1."
#define RT_TEXT_MAX 1024

/* --- SHA-256 (the digest admission_fingerprint is built on) --------------- */
typedef struct {
    uint32_t h[8];
    uint64_t total;
    unsigned char buf[64];
    size_t n;
} rt_sha;

static const uint32_t RT_K[64] = {
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

static uint32_t rt_ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void rt_compress(rt_sha *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rt_ror(w[i - 15], 7) ^ rt_ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rt_ror(w[i - 2], 17) ^ rt_ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = rt_ror(e, 6) ^ rt_ror(e, 11) ^ rt_ror(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + RT_K[i] + w[i];
        uint32_t S0 = rt_ror(a, 2) ^ rt_ror(a, 13) ^ rt_ror(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void rt_sha_init(rt_sha *s) {
    s->h[0] = 0x6a09e667u; s->h[1] = 0xbb67ae85u; s->h[2] = 0x3c6ef372u; s->h[3] = 0xa54ff53au;
    s->h[4] = 0x510e527fu; s->h[5] = 0x9b05688cu; s->h[6] = 0x1f83d9abu; s->h[7] = 0x5be0cd19u;
    s->total = 0;
    s->n = 0;
}

static void rt_sha_update(rt_sha *s, const unsigned char *p, size_t len) {
    s->total += (uint64_t)len;
    while (len) {
        size_t take = 64 - s->n;
        if (take > len) take = len;
        memcpy(s->buf + s->n, p, take);
        s->n += take;
        p += take;
        len -= take;
        if (s->n == 64) { rt_compress(s, s->buf); s->n = 0; }
    }
}

static void rt_sha_hex(const unsigned char *data, size_t len, char out[65]) {
    rt_sha s;
    unsigned char pad[72];
    size_t padlen;
    uint64_t bits;
    int i;

    rt_sha_init(&s);
    rt_sha_update(&s, data, len);
    bits = (uint64_t)len * 8u;
    pad[0] = 0x80;
    padlen = (s.n < 56) ? (56 - s.n) : (120 - s.n);
    memset(pad + 1, 0, padlen - 1);
    rt_sha_update(&s, pad, padlen);
    {
        unsigned char tail[8];
        for (i = 0; i < 8; i++) tail[i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xffu);
        rt_sha_update(&s, tail, 8);
    }
    for (i = 0; i < 8; i++)
        snprintf(out + i * 8, 9, "%08x", s.h[i]);
    out[64] = '\0';
}

/* --- Python value helpers ------------------------------------------------- */
typedef struct { char *b; size_t n, cap; int oom; } rt_buf;

static void rt_bput(rt_buf *s, const char *t, size_t len) {
    if (s->oom) return;
    if (s->n + len + 1 > s->cap) {
        size_t cap = s->cap ? s->cap : 128;
        while (cap < s->n + len + 1) cap *= 2;
        char *nb = (char *)realloc(s->b, cap);
        if (!nb) { s->oom = 1; return; }
        s->b = nb;
        s->cap = cap;
    }
    memcpy(s->b + s->n, t, len);
    s->n += len;
    s->b[s->n] = '\0';
}

static void rt_bputs(rt_buf *s, const char *t) { rt_bput(s, t, strlen(t)); }

static char *rt_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int rt_is_str(const json_t *v) { return v && v->type == JSON_STRING; }

static int rt_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '=='. */
static int rt_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (rt_number(a, &x) && rt_number(b, &y)) return x == y;
    if (rt_is_str(a) && rt_is_str(b))
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

/* json.dumps string body (ensure_ascii=False, the escapes CPython emits). */
static void rt_escape(rt_buf *s, const char *t) {
    for (const char *p = t; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
        case '"':  rt_bput(s, "\\\"", 2); break;
        case '\\': rt_bput(s, "\\\\", 2); break;
        case '\b': rt_bput(s, "\\b", 2); break;
        case '\f': rt_bput(s, "\\f", 2); break;
        case '\n': rt_bput(s, "\\n", 2); break;
        case '\r': rt_bput(s, "\\r", 2); break;
        case '\t': rt_bput(s, "\\t", 2); break;
        default:
            if (c < 0x20) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", c);
                rt_bputs(s, esc);
            } else {
                rt_bput(s, (const char *)p, 1);
            }
            break;
        }
    }
}

/* admission_fingerprint(canonical_target=target, payload=<payload text>) */
static char *rt_fingerprint(const char *target, const char *payload_text) {
    rt_buf s = {NULL, 0, 0, 0};
    char hex[65];
    char *out;

    rt_bputs(&s, "{\"payload\":");
    rt_bputs(&s, payload_text);
    rt_bputs(&s, ",\"target\":\"");
    rt_escape(&s, target ? target : "");
    rt_bputs(&s, "\"}");
    if (s.oom || !s.b) { free(s.b); return NULL; }
    rt_sha_hex((const unsigned char *)s.b, s.n, hex);
    out = rt_dup(hex);
    free(s.b);
    return out;
}

/* _text(value): str, non-empty, at most 1024 characters. NULL when refused. */
static const char *rt_text(const json_t *v) {
    if (!rt_is_str(v)) return NULL;
    if (!v->str_val || v->str_val[0] == '\0') return NULL;
    if (strlen(v->str_val) > RT_TEXT_MAX) return NULL;
    return v->str_val;
}

static json_t *rt_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

/* {"principal":<p>,"request":<r>} -- sort_keys order. */
static char *rt_principal_payload(const char *principal, const char *request) {
    rt_buf s = {NULL, 0, 0, 0};
    rt_bputs(&s, "{\"principal\":\"");
    rt_escape(&s, principal);
    rt_bputs(&s, "\",\"request\":\"");
    rt_escape(&s, request);
    rt_bputs(&s, "\"}");
    if (s.oom) { free(s.b); return NULL; }
    return s.b;
}

json_t *w24_runtimeret_4ec6b1_identity_key(const json_t *principal_id, const json_t *session_id,
                                           const json_t *request_id) {
    const char *p = rt_text(principal_id), *s = rt_text(session_id), *r = rt_text(request_id);
    char *payload, *hex;
    size_t need;
    char *key;
    json_t *out;

    /* for value in (principal_id, session_id, request_id): _text(value) */
    if (!p || !s || !r) return rt_error("invalid_params");
    payload = rt_principal_payload(p, r);
    if (!payload) return rt_error("MemoryError");
    hex = rt_fingerprint(s, payload);
    free(payload);
    if (!hex) return rt_error("MemoryError");
    need = sizeof(RT_IDENTITY_PREFIX) + strlen(hex);
    key = (char *)malloc(need);
    if (!key) { free(hex); return rt_error("MemoryError"); }
    snprintf(key, need, "%s%s", RT_IDENTITY_PREFIX, hex);
    out = json_object();
    json_set(out, "key", json_string(key));
    free(key);
    free(hex);
    return out;
}

json_t *w24_runtimeret_4ec6b1_receipt_key(const json_t *principal_id, const json_t *session_id,
                                          const json_t *request_id) {
    const char *p = rt_text(principal_id), *s = rt_text(session_id), *r = rt_text(request_id);
    char *payload, *key_hex, *digest;
    size_t need;
    char *key;
    json_t *out;

    if (!p || !s || !r) return rt_error("invalid_params");
    payload = rt_principal_payload(p, r);
    if (!payload) return rt_error("MemoryError");
    key_hex = rt_fingerprint(s, payload);
    free(payload);
    if (!key_hex) return rt_error("MemoryError");
    /* digest over {"operation","payload":{},"expected_revision":0} */
    digest = rt_fingerprint(s, "{\"expected_revision\":0,\"operation\":\"model\",\"payload\":{}}");
    if (!digest) { free(key_hex); return rt_error("MemoryError"); }
    need = sizeof(RT_MUTATION_PREFIX) + strlen(key_hex);
    key = (char *)malloc(need);
    if (!key) { free(key_hex); free(digest); return rt_error("MemoryError"); }
    snprintf(key, need, "%s%s", RT_MUTATION_PREFIX, key_hex);
    out = json_object();
    json_set(out, "key", json_string(key));
    json_set(out, "digest", json_string(digest));
    free(key);
    free(key_hex);
    free(digest);
    return out;
}

/* Python 'in' over a list of ids. */
static int rt_contains(const json_t *list, const json_t *value) {
    size_t i, n;
    if (!list || list->type != JSON_ARRAY) return 0;
    n = json_len(list);
    for (i = 0; i < n; i++)
        if (rt_equal(json_get(list, i), value)) return 1;
    return 0;
}

static json_t *rt_at(const json_t *list, size_t i) {
    if (!list || list->type != JSON_ARRAY || i >= json_len(list)) return NULL;
    return json_get(list, i);
}

json_t *w24_runtimeret_4ec6b1_first_generation(const json_t *generations, const json_t *retired,
                                               const json_t *pinned, const json_t *identity_keys,
                                               const json_t *rows, const json_t *start) {
    size_t i, n, idx, limit;
    double start_num = 0.0;
    int has_pin = 0;
    const char *pin_principal = NULL, *pin_request = NULL;
    json_t *out;

    if (!generations || generations->type != JSON_ARRAY) return rt_error("TypeError");
    if (start && start->type != JSON_NULL) {
        if (!rt_number(start, &start_num)) return rt_error("TypeError");
    }
    idx = 0;
    if (start_num > 0.0) idx = (size_t)start_num;
    if (pinned && pinned->type == JSON_ARRAY && json_len(pinned) >= 2 && rt_is_str(json_get(pinned, 0)) &&
        rt_is_str(json_get(pinned, 1))) {
        has_pin = 1;
        pin_principal = json_get(pinned, 0)->str_val;
        pin_request = json_get(pinned, 1)->str_val;
    }

    n = json_len(generations);
    limit = n + 1;   /* the walk is bounded by the candidate list */
    for (i = 0; i < limit; i++) {
        json_t *sid;
        if (idx >= n) return rt_error("IndexError");
        sid = rt_at(generations, idx);
        if (rt_contains(retired, sid)) {
            if (has_pin) {
                /* identity_key(pinned[0], sid, pinned[1]) in state_meta */
                char *payload = rt_principal_payload(pin_principal, pin_request);
                char *hex = payload ? rt_fingerprint(sid && rt_is_str(sid) ? sid->str_val : "", payload) : NULL;
                free(payload);
                if (hex) {
                    size_t need = sizeof(RT_IDENTITY_PREFIX) + strlen(hex);
                    char *key = (char *)malloc(need);
                    int found = 0;
                    if (key) {
                        json_t *probe;
                        snprintf(key, need, "%s%s", RT_IDENTITY_PREFIX, hex);
                        probe = json_string(key);
                        found = rt_contains(identity_keys, probe);
                        json_free(probe);
                        free(key);
                    }
                    free(hex);
                    if (found) {
                        out = json_object();
                        json_set(out, "generation", json_number((double)idx));
                        json_set(out, "sid", sid ? json_copy(sid) : json_null());
                        json_set(out, "row", json_null());
                        return out;
                    }
                }
            }
            idx++;
            continue;
        }
        out = json_object();
        json_set(out, "generation", json_number((double)idx));
        json_set(out, "sid", sid ? json_copy(sid) : json_null());
        {
            const json_t *row = NULL;
            if (rows && rows->type == JSON_OBJECT && sid && rt_is_str(sid)) row = json_obj_get(rows, sid->str_val);
            json_set(out, "row", row ? json_copy(row) : json_null());
        }
        return out;
    }
    return rt_error("IndexError");
}

json_t *w24_runtimeret_4ec6b1_keep_passed_fences(const json_t *generations, const json_t *session_id,
                                                 const json_t *marked) {
    json_t *out, *unexpired;
    size_t i, n;
    int ok = 1;

    if (!generations || generations->type != JSON_ARRAY) return rt_error("TypeError");
    out = json_object();
    unexpired = json_array();
    n = json_len(generations);
    for (i = 0; i <= n; i++) {          /* bounded by the candidate list */
        const json_t *sid;
        char *key;
        size_t need;
        json_t *marker;
        if (i >= n) {
            /* derive() ran out before reaching session_id: IndexError */
            json_free(unexpired);
            json_free(out);
            return rt_error("IndexError");
        }
        sid = rt_at(generations, i);
        if (rt_equal(sid, session_id)) break;
        /* conn.execute('SELECT 1 FROM state_meta WHERE key=?', (RETIRED_PREFIX + sid,)) */
        need = sizeof(RT_RETIRED_PREFIX) + (sid && rt_is_str(sid) ? strlen(sid->str_val) : 0);
        key = (char *)malloc(need);
        if (!key) {
            json_free(unexpired);
            json_free(out);
            return rt_error("MemoryError");
        }
        snprintf(key, need, "%s%s", RT_RETIRED_PREFIX, (sid && rt_is_str(sid)) ? sid->str_val : "");
        marker = json_string(key);
        free(key);
        if (!rt_contains(marked, marker)) {
            json_free(marker);
            ok = 0;
            break;                      /* a walk now stops here: not a successor */
        }
        json_free(marker);
        /* the UPDATE drops 'expires' from the marker: the fence becomes permanent */
        json_append(unexpired, sid ? json_copy(sid) : json_null());
    }
    json_set(out, "ok", json_bool(ok));
    json_set(out, "unexpired", unexpired);
    return out;
}

json_t *w24_runtimeret_4ec6b1_constants(void) {
    json_t *o = json_object();
    json_set(o, "retired_prefix", json_string(RT_RETIRED_PREFIX));
    json_set(o, "identity_prefix", json_string(RT_IDENTITY_PREFIX));
    json_set(o, "mutation_prefix", json_string(RT_MUTATION_PREFIX));
    json_set(o, "admission_prefix", json_string(RT_ADMISSION_PREFIX));
    json_set(o, "text_max_chars", json_number(RT_TEXT_MAX));
    return o;
}
