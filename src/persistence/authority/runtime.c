/*
 * runtime.c - C11 port of upstream persistence/authority/runtime.py
 * (gates w33_runtime_dd441a_p1, w33_runtime_dd441a_p2).
 *
 * PoP: w33_runtime_dd441a_p1_mutation_receipt_key @ persistence/authority/runtime.py:mutation_receipt_key
 * PoP: w33_runtime_dd441a_p1_chat_pick_key        @ persistence/authority/runtime.py:chat_pick_key
 * PoP: w33_runtime_dd441a_p1_text                 @ persistence/authority/runtime.py:_text
 * PoP: w33_runtime_dd441a_p1_constants            @ persistence/authority/runtime.py:COMPACTED_TARGET_DETAIL
 * PoP: w33_runtime_dd441a_p2_parse_mutation       @ persistence/authority/runtime.py:parse_mutation
 * PoP: w33_runtime_dd441a_p2_reasoning_level      @ persistence/authority/runtime.py:_reasoning_level
 * PoP: w33_runtime_dd441a_p2_editor_fingerprint   @ persistence/authority/runtime.py:_editor_fingerprint
 * PoP: w33_runtime_dd441a_p2_generation_required  @ persistence/authority/runtime.py:_GENERATION_REQUIRED
 * PoP: w33_runtime_dd441a_p2_metadata_fields      @ persistence/authority/runtime.py:METADATA_FIELDS
 * PoP: w33_runtime_dd441a_p2_constants            @ persistence/authority/runtime.py:<module constants>
 * PoP: rt_fingerprint                             @ persistence/authority/vocabulary.py:admission_fingerprint
 * PoP: rt_sb_value                                @ persistence/core/canonical.py:canonical_dumps
 *
 * What is ported here is the part of the runtime authority that is a pure
 * function of values: the exact-retry receipt identity (the canonical
 * ``{"target", "payload"}`` digest ``admission_fingerprint`` computes, over the
 * receipt key's identity payload and over the mutation fingerprint), the
 * chat-pick spelling of it with its fence checks, the closed MutationCommand
 * union validation with every command's wire payload, and the predicates the
 * metadata/compress rules read.
 *
 * Seams (never touched here): the store transaction, the authority epoch and
 * its ownership fence, receipts rows, the session/branch rows, the ledger and
 * the clock.  They arrive as fixture values into the two gate surfaces.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w33_runtime_dd441a_p1.h"
#include "noc_w33_runtime_dd441a_p2.h"

/* ------------------------------------------------------------------ */
/* A growable byte buffer for the canonical encoding.                  */
/* ------------------------------------------------------------------ */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} rt_sb;

static void rt_sb_init(rt_sb *b) { b->p = NULL; b->n = 0; b->cap = 0; }

static void rt_sb_free(rt_sb *b) {
    free(b->p);
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

static int rt_sb_reserve(rt_sb *b, size_t extra) {
    size_t need = b->n + extra + 1;
    size_t cap;
    char *np;
    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 128;
    while (cap < need) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) return 0;
    b->p = np;
    b->cap = cap;
    return 1;
}

static void rt_sb_addn(rt_sb *b, const char *s, size_t n) {
    if (!rt_sb_reserve(b, n)) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void rt_sb_add(rt_sb *b, const char *s) { rt_sb_addn(b, s, strlen(s)); }

static void rt_sb_ch(rt_sb *b, char c) { rt_sb_addn(b, &c, 1); }

/* ------------------------------------------------------------------ */
/* Python's json.dumps(..., ensure_ascii=False, sort_keys=True,        */
/* separators=(",", ":")) over the values a JSON document holds.       */
/* ------------------------------------------------------------------ */

/* A number as json.dumps writes it: an int literal for an integral value,
 * else Python's shortest round-tripping repr. */
static void rt_sb_num(rt_sb *b, double v) {
    char tmp[64];
    int prec;
    if (v != v || v > 1e308 || v < -1e308) {
        rt_sb_add(b, "null");
        return;
    }
    if (v == floor(v) && v > -9.0e15 && v < 9.0e15) {
        snprintf(tmp, sizeof tmp, "%lld", (long long)v);
        rt_sb_add(b, tmp);
        return;
    }
    for (prec = 15; prec <= 17; prec++) {
        snprintf(tmp, sizeof tmp, "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    rt_sb_add(b, tmp);
}

static void rt_sb_str(rt_sb *b, const char *s) {
    const unsigned char *p;
    rt_sb_ch(b, '"');
    for (p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"':  rt_sb_add(b, "\\\""); break;
        case '\\': rt_sb_add(b, "\\\\"); break;
        case '\b': rt_sb_add(b, "\\b"); break;
        case '\f': rt_sb_add(b, "\\f"); break;
        case '\n': rt_sb_add(b, "\\n"); break;
        case '\r': rt_sb_add(b, "\\r"); break;
        case '\t': rt_sb_add(b, "\\t"); break;
        default:
            if (*p < 0x20) {
                char tmp[8];
                snprintf(tmp, sizeof tmp, "\\u%04x", (unsigned)*p);
                rt_sb_add(b, tmp);
            } else {
                rt_sb_ch(b, (char)*p);
            }
        }
    }
    rt_sb_ch(b, '"');
}

static void rt_sb_value(rt_sb *b, const json_t *v);

/* Sorted keys, last occurrence of a repeated key wins (json.loads keeps the
 * last value of a duplicate), compact separators. */
static void rt_sb_obj(rt_sb *b, const json_t *v) {
    size_t n = json_object_size(v);
    size_t *order;
    size_t i, j, m = 0;
    if (n == 0) {
        rt_sb_add(b, "{}");
        return;
    }
    order = (size_t *)malloc(n * sizeof(size_t));
    if (!order) {
        rt_sb_add(b, "{}");
        return;
    }
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(v, i);
        int shadowed = 0;
        size_t pos;
        for (j = i + 1; j < n && !shadowed; j++) {
            const char *later = json_object_get_key_at(v, j);
            if (k && later && !strcmp(k, later)) shadowed = 1;
        }
        if (shadowed) continue;
        /* insertion sort by key bytes (UTF-8 byte order == code point order) */
        pos = m;
        while (pos > 0) {
            const char *prev = json_object_get_key_at(v, order[pos - 1]);
            if (strcmp(prev ? prev : "", k ? k : "") <= 0) break;
            order[pos] = order[pos - 1];
            pos--;
        }
        order[pos] = i;
        m++;
    }
    rt_sb_ch(b, '{');
    for (i = 0; i < m; i++) {
        const char *k = json_object_get_key_at(v, order[i]);
        if (i) rt_sb_ch(b, ',');
        rt_sb_str(b, k ? k : "");
        rt_sb_ch(b, ':');
        rt_sb_value(b, json_object_get_at(v, order[i]));
    }
    rt_sb_ch(b, '}');
    free(order);
}

static void rt_sb_value(rt_sb *b, const json_t *v) {
    size_t i, n;
    if (!v) {
        rt_sb_add(b, "null");
        return;
    }
    switch (v->type) {
    case JSON_NULL:   rt_sb_add(b, "null"); break;
    case JSON_BOOL:   rt_sb_add(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: rt_sb_num(b, v->num_val); break;
    case JSON_STRING: rt_sb_str(b, v->str_val ? v->str_val : ""); break;
    case JSON_ARRAY:
        rt_sb_ch(b, '[');
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) rt_sb_ch(b, ',');
            rt_sb_value(b, json_get(v, i));
        }
        rt_sb_ch(b, ']');
        break;
    case JSON_OBJECT: rt_sb_obj(b, v); break;
    default: rt_sb_add(b, "null"); break;
    }
}

/* ------------------------------------------------------------------ */
/* SHA-256 (FIPS 180-4) — the digest admission_fingerprint returns.    */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t h[8];
    uint64_t total;
    unsigned char block[64];
    size_t fill;
} rt_sha;

static const uint32_t RT_SHA_K[64] = {
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

static uint32_t rt_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void rt_sha_init(rt_sha *c) {
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u; c->h[2] = 0x3c6ef372u;
    c->h[3] = 0xa54ff53au; c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu;
    c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->total = 0;
    c->fill = 0;
}

static void rt_sha_block(rt_sha *c, const unsigned char *p) {
    uint32_t w[64], a, b2, cc, d, e, f, g, h, s0, s1, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t x = w[i - 15], y = w[i - 2];
        s0 = rt_rotr(x, 7) ^ rt_rotr(x, 18) ^ (x >> 3);
        s1 = rt_rotr(y, 17) ^ rt_rotr(y, 19) ^ (y >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->h[0]; b2 = c->h[1]; cc = c->h[2]; d = c->h[3];
    e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (i = 0; i < 64; i++) {
        s1 = rt_rotr(e, 6) ^ rt_rotr(e, 11) ^ rt_rotr(e, 25);
        t1 = h + s1 + ((e & f) ^ ((~e) & g)) + RT_SHA_K[i] + w[i];
        s0 = rt_rotr(a, 2) ^ rt_rotr(a, 13) ^ rt_rotr(a, 22);
        t2 = s0 + ((a & b2) ^ (a & cc) ^ (b2 & cc));
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b2; b2 = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b2; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void rt_sha_update(rt_sha *c, const unsigned char *p, size_t n) {
    c->total += (uint64_t)n;
    while (n > 0) {
        size_t take = 64 - c->fill;
        if (take > n) take = n;
        memcpy(c->block + c->fill, p, take);
        c->fill += take;
        p += take;
        n -= take;
        if (c->fill == 64) {
            rt_sha_block(c, c->block);
            c->fill = 0;
        }
    }
}

static void rt_sha_final(rt_sha *c, unsigned char out[32]) {
    uint64_t bits = c->total * 8;
    unsigned char pad[72];
    size_t padn, i;
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    padn = (c->fill < 56) ? (56 - c->fill) : (120 - c->fill);
    for (i = 0; i < 8; i++) pad[padn + i] = (unsigned char)(bits >> (56 - 8 * i));
    rt_sha_update(c, pad, padn + 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)c->h[i];
    }
}

/* admission_fingerprint(canonical_target=t, payload=p): sha256 hex of the
 * canonical {"payload": p, "target": t}. */
static char *rt_fingerprint(const json_t *target, const json_t *payload) {
    rt_sb b;
    rt_sha c;
    unsigned char d[32];
    char *hex;
    int i;
    rt_sb_init(&b);
    rt_sb_add(&b, "{\"payload\":");
    rt_sb_value(&b, payload);
    rt_sb_add(&b, ",\"target\":");
    rt_sb_value(&b, target);
    rt_sb_ch(&b, '}');
    hex = (char *)malloc(65);
    if (!hex) {
        rt_sb_free(&b);
        return NULL;
    }
    rt_sha_init(&c);
    rt_sha_update(&c, (const unsigned char *)(b.p ? b.p : ""), b.n);
    rt_sha_final(&c, d);
    for (i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", d[i]);
    rt_sb_free(&b);
    return hex;
}

/* ------------------------------------------------------------------ */
/* Python view helpers over the parsed JSON tree.                      */
/* ------------------------------------------------------------------ */

static int rt_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int rt_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static int rt_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int rt_is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }

/* A fixture integer: the JSON number spelling of a Python int. */
static int rt_is_int(const json_t *v) {
    return v && v->type == JSON_NUMBER && v->num_val == floor(v->num_val) &&
           fabs(v->num_val) < 9.0e15;
}

static int rt_has(const json_t *o, const char *k) {
    return o && o->type == JSON_OBJECT && json_obj_get(o, k) != NULL;
}

static size_t rt_size(const json_t *o) { return json_object_size(o); }

static int rt_in(const char *s, const char *const *set, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (set[i] && s && !strcmp(set[i], s)) return 1;
    return 0;
}

/* Every key of the object is one of ``keys``. */
static int rt_only(const json_t *o, const char *const *keys, size_t n) {
    size_t i;
    for (i = 0; i < rt_size(o); i++) {
        const char *k = json_object_get_key_at(o, i);
        if (!k || !rt_in(k, keys, n)) return 0;
    }
    return 1;
}

static int rt_py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() truth: does the string hold a non-whitespace character. */
static int rt_nonblank(const char *s) {
    const char *p = s ? s : "";
    while (*p && rt_py_space((unsigned char)*p)) p++;
    return *p != '\0';
}

/* Python's len() of a str: code points, not bytes. */
static size_t rt_str_len(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    for (; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* _text: a non-empty str of at most 1024 characters. */
static int rt_text_ok(const json_t *v) {
    size_t n;
    if (!rt_is_str(v)) return 0;
    n = rt_str_len(v->str_val);
    return n >= 1 && n <= W33_RUNTIME_DD441A_P1_TEXT_MAX;
}

/* _reasoning_level: value.strip().lower() in the accepted levels. */
static int rt_reasoning_level(const char *s) {
    static const char *const levels[] = {
        "minimal", "low", "medium", "high", "xhigh", "max", "ultra",
    };
    static const char *const off[] = {"none", "false", "disabled"};
    char buf[64];
    size_t n = 0, i;
    const char *p = s ? s : "", *end;
    while (*p && rt_py_space((unsigned char)*p)) p++;
    end = p + strlen(p);
    while (end > p && rt_py_space((unsigned char)end[-1])) end--;
    if ((size_t)(end - p) >= sizeof buf) return 0;
    for (; p < end; p++, n++)
        buf[n] = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
    buf[n] = '\0';
    for (i = 0; i < sizeof levels / sizeof levels[0]; i++)
        if (!strcmp(buf, levels[i])) return 1;
    for (i = 0; i < sizeof off / sizeof off[0]; i++)
        if (!strcmp(buf, off[i])) return 1;
    return 0;
}

static json_t *rt_refusal(void) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string("invalid_params"));
    return o;
}

/* ------------------------------------------------------------------ */
/* Gate p1: receipt identity.                                          */
/* ------------------------------------------------------------------ */

static json_t *rt_receipt(json_t *key_json, json_t *digest_json) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "key", key_json ? key_json : json_null());
    json_set(o, "digest", digest_json ? digest_json : json_null());
    return o;
}

/* The shared body: ``mutation_receipt_key`` over values already read out. */
static json_t *rt_receipt_key(const json_t *principal, const json_t *session,
                              const json_t *request, const json_t *operation,
                              const json_t *payload, const json_t *revision,
                              const json_t *generation, int with_generation) {
    json_t *ident, *fp, *key_json, *digest_json;
    char *ident_hex, *digest_hex, *key_text;

    ident = json_object();
    json_set(ident, "principal", principal ? json_copy(principal) : json_null());
    json_set(ident, "request", request ? json_copy(request) : json_null());
    ident_hex = rt_fingerprint(session, ident);
    json_free(ident);
    if (!ident_hex) return rt_refusal();

    fp = json_object();
    json_set(fp, "operation", operation ? json_copy(operation) : json_null());
    json_set(fp, "payload", payload ? json_copy(payload) : json_null());
    json_set(fp, "expected_revision", revision ? json_copy(revision) : json_null());
    if (with_generation && generation)
        json_set(fp, "expected_generation", json_copy(generation));
    digest_hex = rt_fingerprint(session, fp);
    json_free(fp);
    if (!digest_hex) {
        free(ident_hex);
        return rt_refusal();
    }

    key_text = (char *)malloc(strlen(W33_RUNTIME_DD441A_P1_RECEIPT_PREFIX) + 65);
    if (!key_text) {
        free(ident_hex);
        free(digest_hex);
        return rt_refusal();
    }
    strcpy(key_text, W33_RUNTIME_DD441A_P1_RECEIPT_PREFIX);
    strcat(key_text, ident_hex);
    key_json = json_string(key_text);
    digest_json = json_string(digest_hex);
    free(key_text);
    free(ident_hex);
    free(digest_hex);
    return rt_receipt(key_json, digest_json);
}

json_t *w33_runtime_dd441a_p1_mutation_receipt_key(const json_t *req) {
    const json_t *generation;
    int with_generation;
    if (!rt_is_dict(req)) return rt_refusal();
    generation = json_obj_get(req, "expected_generation");
    with_generation = generation && generation->type != JSON_NULL;
    return rt_receipt_key(json_obj_get(req, "principal_id"),
                          json_obj_get(req, "session_id"),
                          json_obj_get(req, "request_id"),
                          json_obj_get(req, "operation"),
                          json_obj_get(req, "payload"),
                          json_obj_get(req, "expected_revision"),
                          generation, with_generation);
}

json_t *w33_runtime_dd441a_p1_chat_pick_key(const json_t *req) {
    const json_t *principal, *session, *request, *rev, *gen, *payload;
    if (!rt_is_dict(req)) return rt_refusal();
    principal = json_obj_get(req, "principal_id");
    session = json_obj_get(req, "session_id");
    request = json_obj_get(req, "request_id");
    if (!rt_text_ok(principal) || !rt_text_ok(session) || !rt_text_ok(request))
        return rt_refusal();
    rev = json_obj_get(req, "expected_revision");
    gen = json_obj_get(req, "expected_generation");
    if (!rt_is_int(rev) || rev->num_val < 0) return rt_refusal();
    if (!rt_is_int(gen) || gen->num_val < 0) return rt_refusal();
    payload = json_obj_get(req, "payload");
    /* canonical_json refuses a non-dict payload (and a NaN inside it). */
    if (!rt_is_dict(payload)) return rt_refusal();
    return rt_receipt_key(principal, session, request,
                          json_obj_get(req, "operation"), payload, rev, gen, 1);
}

json_t *w33_runtime_dd441a_p1_text(const json_t *req) {
    const json_t *value;
    json_t *o;
    if (!rt_is_dict(req)) return rt_refusal();
    value = json_obj_get(req, "value");
    if (!rt_text_ok(value)) return rt_refusal();
    o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", json_copy(value));
    json_set(o, "length", json_number((double)rt_str_len(value->str_val)));
    return o;
}

json_t *w33_runtime_dd441a_p1_constants(void) {
    json_t *o = json_object();
    json_set(o, "compacted_target_detail",
             json_string("compacted: target user message was summarized when the "
                         "session was compressed"));
    return o;
}

/* ------------------------------------------------------------------ */
/* Gate p2: the closed MutationCommand union.                          */
/* ------------------------------------------------------------------ */

static const char *const RT_METADATA_KEYS[W33_RUNTIME_DD441A_P2_METADATA_FIELDS] = {
    "title", "archived", "hidden", "pinned", "unread",
};

static const char *const RT_REASONING_EFFORTS[W33_RUNTIME_DD441A_P2_REASONING_EFFORTS] = {
    "minimal", "low", "medium", "high", "xhigh", "max", "ultra",
};

static const char *const RT_GENERATION_REQUIRED[W33_RUNTIME_DD441A_P2_GENERATION_REQUIRED] = {
    "branch", "compress", "delete", "model", "reset", "rewind",
};

static const char *const RT_COMPRESS_KEYS[W33_RUNTIME_DD441A_P2_COMPRESS_FIELDS] = {
    "focus", "preview", "partial", "keep_last",
};

static const char *const RT_MODEL_KEYS[4] = {"model", "provider", "confirm", "reasoning"};
static const char *const RT_BRANCH_KEYS[3] = {"title", "through_message_id", "mcp_servers"};
static const char *const RT_REWIND_KEYS[2] = {"target_message_id", "preserve_compaction_handoff"};
static const char *const RT_EDIT_POLICIES[3] = {"ask", "workspace_session", "session"};

/* _editor_fingerprint: a dict whose only key is a str "fingerprint". */
static int rt_editor_fp(const json_t *v, const json_t **out) {
    if (!rt_is_dict(v) || rt_size(v) != 1 || !rt_has(v, "fingerprint")) return 0;
    if (!rt_is_str(json_obj_get(v, "fingerprint"))) return 0;
    if (out) *out = json_obj_get(v, "fingerprint");
    return 1;
}

/* Copy ``keys`` present in ``src`` into a fresh object, in the command's
 * declaration order — the ``payload()`` of a _Command subclass. */
static json_t *rt_ordered_payload(const json_t *src, const char *const *keys, size_t n) {
    json_t *o = json_object();
    size_t i;
    for (i = 0; i < n; i++)
        if (rt_has(src, keys[i])) json_set(o, keys[i], json_copy(json_obj_get(src, keys[i])));
    return o;
}

/* model: every present value a non-blank str, and a reasoning level if given. */
static json_t *rt_parse_model(const json_t *p, const char **cmd) {
    size_t i;
    if (!rt_has(p, "model") || !rt_only(p, RT_MODEL_KEYS, 4)) return NULL;
    for (i = 0; i < rt_size(p); i++) {
        const json_t *v = json_object_get_at(p, i);
        if (!rt_is_str(v) || !rt_nonblank(v->str_val)) return NULL;
    }
    if (rt_has(p, "reasoning") &&
        !rt_reasoning_level(json_obj_get(p, "reasoning")->str_val))
        return NULL;
    *cmd = "ModelPick";
    return rt_ordered_payload(p, RT_MODEL_KEYS, 4);
}

static json_t *rt_parse_branch(const json_t *p, const char **cmd) {
    const json_t *title, *through, *fp = NULL;
    json_t *o;
    if (!rt_only(p, RT_BRANCH_KEYS, 3)) return NULL;
    if (rt_has(p, "mcp_servers") && !rt_editor_fp(json_obj_get(p, "mcp_servers"), &fp))
        return NULL;
    title = json_obj_get(p, "title");
    if (rt_has(p, "title") && !rt_is_str(title)) return NULL;
    through = json_obj_get(p, "through_message_id");
    if (rt_has(p, "through_message_id") && !(rt_is_int(through) && through->num_val > 0))
        return NULL;
    o = json_object();
    if (title) json_set(o, "title", json_copy(title));
    if (through) json_set(o, "through_message_id", json_copy(through));
    if (fp) {
        json_t *servers = json_object();
        json_set(servers, "fingerprint", json_copy(fp));
        json_set(o, "mcp_servers", servers);
    }
    *cmd = "Branch";
    return o;
}

/* the reasoning / workspace / editor_mcp / edit_policy payload rules. */
static json_t *rt_parse_policy(const char *op, const json_t *p, const char **cmd) {
    json_t *o;
    if (!strcmp(op, "reasoning") && rt_size(p) == 1 && rt_has(p, "effort")) {
        const json_t *effort = json_obj_get(p, "effort");
        if (effort->type != JSON_NULL &&
            !(rt_is_str(effort) && rt_reasoning_level(effort->str_val)))
            return NULL;
        o = json_object();
        json_set(o, "effort", json_copy(effort));
        *cmd = "ReasoningPick";
        return o;
    }
    if (!strcmp(op, "workspace") && rt_size(p) == 1 && rt_has(p, "cwd")) {
        const json_t *cwd = json_obj_get(p, "cwd");
        if (!rt_is_str(cwd) || !rt_nonblank(cwd->str_val)) return NULL;
        o = json_object();
        json_set(o, "cwd", json_copy(cwd));
        *cmd = "Workspace";
        return o;
    }
    if (!strcmp(op, "editor_mcp") && rt_size(p) == 1 && rt_has(p, "mcp_servers")) {
        const json_t *fp = NULL;
        if (!rt_editor_fp(json_obj_get(p, "mcp_servers"), &fp)) return NULL;
        o = json_object();
        {
            json_t *servers = json_object();
            json_set(servers, "fingerprint", json_copy(fp));
            json_set(o, "mcp_servers", servers);
        }
        *cmd = "EditorMcp";
        return o;
    }
    if (!strcmp(op, "edit_policy") && rt_size(p) == 1 && rt_has(p, "policy")) {
        const json_t *policy = json_obj_get(p, "policy");
        if (!rt_is_str(policy) || !rt_in(policy->str_val, RT_EDIT_POLICIES, 3))
            return NULL;
        o = json_object();
        json_set(o, "policy", json_copy(policy));
        *cmd = "EditPolicy";
        return o;
    }
    return NULL;
}

static json_t *rt_parse_compress(const json_t *p, const char **cmd) {
    size_t i;
    const json_t *partial;
    if (!rt_only(p, RT_COMPRESS_KEYS, W33_RUNTIME_DD441A_P2_COMPRESS_FIELDS))
        return NULL;
    for (i = 0; i < rt_size(p); i++) {
        const char *k = json_object_get_key_at(p, i);
        const json_t *v = json_object_get_at(p, i);
        int ok = 0;
        if (!strcmp(k, "focus")) ok = rt_is_str(v);
        else if (!strcmp(k, "preview") || !strcmp(k, "partial")) ok = rt_is_bool(v);
        else if (!strcmp(k, "keep_last")) ok = rt_is_int(v);
        if (!ok) return NULL;
    }
    if (rt_has(p, "keep_last")) {
        partial = json_obj_get(p, "partial");
        if (!(partial && rt_is_bool(partial) && partial->bool_val &&
              json_obj_get(p, "keep_last")->num_val >= 1))
            return NULL;
    }
    *cmd = "Compress";
    return rt_ordered_payload(p, RT_COMPRESS_KEYS, W33_RUNTIME_DD441A_P2_COMPRESS_FIELDS);
}

static json_t *rt_parse_transcript(const char *op, const json_t *p, const char **cmd) {
    if ((!strcmp(op, "delete") || !strcmp(op, "reset")) && rt_size(p) == 0) {
        *cmd = !strcmp(op, "delete") ? "Delete" : "Reset";
        return json_object();
    }
    if (!strcmp(op, "import") && rt_size(p) == 1 && rt_has(p, "sessions")) {
        const json_t *sessions = json_obj_get(p, "sessions");
        json_t *o, *list;
        size_t i;
        if (!rt_is_list(sessions)) return NULL;
        list = json_array();
        for (i = 0; i < json_len(sessions); i++) {
            const json_t *item = json_get(sessions, i);
            if (!rt_is_dict(item)) {
                json_free(list);
                return NULL; /* dict(item) on a non-mapping raises */
            }
            json_append(list, json_copy(item));
        }
        o = json_object();
        json_set(o, "sessions", list);
        *cmd = "Import";
        return o;
    }
    if (!strcmp(op, "rewind") && rt_has(p, "target_message_id") &&
        rt_only(p, RT_REWIND_KEYS, 2)) {
        const json_t *target = json_obj_get(p, "target_message_id");
        const json_t *preserve = json_obj_get(p, "preserve_compaction_handoff");
        json_t *o;
        if (!(rt_is_int(target) && target->num_val > 0)) return NULL;
        if (preserve && !rt_is_bool(preserve)) return NULL;
        o = json_object();
        json_set(o, "target_message_id", json_copy(target));
        if (preserve) json_set(o, "preserve_compaction_handoff", json_copy(preserve));
        *cmd = "Rewind";
        return o;
    }
    return NULL;
}

/* rename / archive / sidebar: the metadata edits, and the refusal every other
 * operation falls through to. */
static json_t *rt_parse_metadata(const char *op, const json_t *p, const char **cmd) {
    int valid;
    size_t i;
    if (!strcmp(op, "sidebar"))
        valid = rt_size(p) > 0 &&
                rt_only(p, RT_METADATA_KEYS, W33_RUNTIME_DD441A_P2_METADATA_FIELDS);
    else if (!strcmp(op, "rename"))
        valid = rt_size(p) == 1 && rt_has(p, "title");
    else if (!strcmp(op, "archive"))
        valid = rt_size(p) == 1 && rt_has(p, "archived");
    else
        valid = 0;
    if (!valid) return NULL;
    for (i = 0; i < rt_size(p); i++) {
        const char *k = json_object_get_key_at(p, i);
        const json_t *v = json_object_get_at(p, i);
        if (!strcmp(k, "title")) {
            if (!rt_is_str(v)) return NULL;
        } else if (!rt_is_bool(v)) {
            return NULL;
        }
    }
    if (!strcmp(op, "rename")) *cmd = "Rename";
    else if (!strcmp(op, "archive")) *cmd = "Archive";
    else *cmd = "Sidebar";
    return rt_ordered_payload(p, RT_METADATA_KEYS, W33_RUNTIME_DD441A_P2_METADATA_FIELDS);
}

json_t *w33_runtime_dd441a_p2_parse_mutation(const json_t *operation, const json_t *payload) {
    const char *op, *cmd = NULL;
    json_t *parsed = NULL, *o;
    if (!rt_is_dict(payload) || !rt_is_str(operation)) return rt_refusal();
    op = operation->str_val ? operation->str_val : "";
    if (!strcmp(op, "model")) parsed = rt_parse_model(payload, &cmd);
    else if (!strcmp(op, "branch")) parsed = rt_parse_branch(payload, &cmd);
    else if (!strcmp(op, "compress")) parsed = rt_parse_compress(payload, &cmd);
    else if (!strcmp(op, "reasoning") || !strcmp(op, "workspace") ||
             !strcmp(op, "editor_mcp") || !strcmp(op, "edit_policy"))
        parsed = rt_parse_policy(op, payload, &cmd);
    else if (!strcmp(op, "delete") || !strcmp(op, "reset") ||
             !strcmp(op, "import") || !strcmp(op, "rewind"))
        parsed = rt_parse_transcript(op, payload, &cmd);
    if (!parsed) parsed = rt_parse_metadata(op, payload, &cmd);
    if (!parsed || !cmd) {
        if (parsed) json_free(parsed);
        return rt_refusal();
    }
    o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "command", json_string(cmd));
    json_set(o, "payload", parsed);
    return o;
}

json_t *w33_runtime_dd441a_p2_reasoning_level(const json_t *value) {
    if (!rt_is_str(value)) return json_bool(false);
    return json_bool(rt_reasoning_level(value->str_val) ? true : false);
}

json_t *w33_runtime_dd441a_p2_editor_fingerprint(const json_t *value) {
    const json_t *fp = NULL;
    if (!rt_editor_fp(value, &fp)) return json_null();
    return json_copy(fp);
}

json_t *w33_runtime_dd441a_p2_generation_required(const json_t *operation) {
    if (!rt_is_str(operation)) return json_bool(false);
    return json_bool(rt_in(operation->str_val, RT_GENERATION_REQUIRED,
                           W33_RUNTIME_DD441A_P2_GENERATION_REQUIRED)
                         ? true
                         : false);
}

json_t *w33_runtime_dd441a_p2_metadata_fields(void) {
    json_t *o = json_object();
    json_set(o, "title", json_string("str"));
    json_set(o, "archived", json_string("bool"));
    json_set(o, "hidden", json_string("bool"));
    json_set(o, "pinned", json_string("bool"));
    json_set(o, "unread", json_string("bool"));
    return o;
}

json_t *w33_runtime_dd441a_p2_constants(void) {
    json_t *o = json_object();
    json_t *efforts = json_array();
    json_t *required = json_array();
    json_t *compress = json_object();
    size_t i;

    for (i = 0; i < W33_RUNTIME_DD441A_P2_REASONING_EFFORTS; i++)
        json_append(efforts, json_string(RT_REASONING_EFFORTS[i]));
    for (i = 0; i < W33_RUNTIME_DD441A_P2_GENERATION_REQUIRED; i++)
        json_append(required, json_string(RT_GENERATION_REQUIRED[i]));

    json_set(compress, "focus", json_string("str"));
    json_set(compress, "preview", json_string("bool"));
    json_set(compress, "partial", json_string("bool"));
    json_set(compress, "keep_last", json_string("int"));

    json_set(o, "reasoning_efforts", efforts);
    json_set(o, "generation_required", required);
    json_set(o, "compress_fields", compress);
    json_set(o, "metadata_fields", w33_runtime_dd441a_p2_metadata_fields());
    return o;
}
