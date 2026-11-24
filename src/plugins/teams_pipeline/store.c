/*
 * store.c -- real C11 port of the durable state decisions of upstream Hermes
 *   plugins/teams_pipeline/store.py.
 *
 * TeamsPipelineStore is a JSON document behind an RLock: reads deepcopy a bucket,
 * writes merge a record, stamp timestamps and atomically replace the file. The
 * lock, the NamedTemporaryFile replace and get_hermes_home() path resolution are
 * plumbing; what lands *in* the document is decided by the pure functions ported
 * here -- the load normalisation, the upsert merge, the notification receipt key
 * (a sha256 over Python's canonical json) and the receipt de-duplication.
 *
 * PoP: w20_store_ef85f3_load_state @ plugins/teams_pipeline/store.py:TeamsPipelineStore._load
 * PoP: w20_store_ef85f3_receipt_key @ plugins/teams_pipeline/store.py:TeamsPipelineStore.build_notification_receipt_key
 * PoP: w20_store_ef85f3_upsert @ plugins/teams_pipeline/store.py:TeamsPipelineStore._upsert
 * PoP: w20_store_ef85f3_record_receipt @ plugins/teams_pipeline/store.py:TeamsPipelineStore.record_notification_receipt
 */
#include "hermes_json.h"
#include "noc_w20_store_ef85f3.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const STORE_BUCKETS[W20_STORE_EF85F3_BUCKET_COUNT] = {
    "subscriptions", "notification_receipts", "event_timestamps", "jobs", "sink_records"
};

static char *st85_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

const char *w20_store_ef85f3_bucket_name(int i) {
    if (i < 0 || i >= W20_STORE_EF85F3_BUCKET_COUNT) return NULL;
    return STORE_BUCKETS[i];
}

int w20_store_ef85f3_is_bucket(const char *name) {
    int i;
    if (!name) return 0;
    for (i = 0; i < W20_STORE_EF85F3_BUCKET_COUNT; i++)
        if (!strcmp(name, STORE_BUCKETS[i])) return 1;
    return 0;
}

/* --- SHA-256 (FIPS 180-4), no dependency -------------------------------- */

typedef struct {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t used;
} st85_sha;

static uint32_t st85_ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void st85_sha_block(st85_sha *s, const unsigned char *p) {
    static const uint32_t K[64] = {
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
    uint32_t w[64], a, b, c, d, e, f, g, h;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = st85_ror(w[i - 15], 7) ^ st85_ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = st85_ror(w[i - 2], 17) ^ st85_ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = st85_ror(e, 6) ^ st85_ror(e, 11) ^ st85_ror(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = st85_ror(a, 2) ^ st85_ror(a, 13) ^ st85_ror(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void st85_sha_init(st85_sha *s) {
    s->h[0] = 0x6a09e667u; s->h[1] = 0xbb67ae85u; s->h[2] = 0x3c6ef372u; s->h[3] = 0xa54ff53au;
    s->h[4] = 0x510e527fu; s->h[5] = 0x9b05688cu; s->h[6] = 0x1f83d9abu; s->h[7] = 0x5be0cd19u;
    s->len = 0;
    s->used = 0;
}

static void st85_sha_update(st85_sha *s, const unsigned char *p, size_t n) {
    s->len += (uint64_t)n;
    while (n) {
        size_t take = 64 - s->used;
        if (take > n) take = n;
        memcpy(s->buf + s->used, p, take);
        s->used += take;
        p += take;
        n -= take;
        if (s->used == 64) {
            st85_sha_block(s, s->buf);
            s->used = 0;
        }
    }
}

static void st85_sha_final(st85_sha *s, unsigned char out[32]) {
    uint64_t bits = s->len * 8u;
    unsigned char pad = 0x80;
    unsigned char lenb[8];
    int i;
    st85_sha_update(s, &pad, 1);
    pad = 0x00;
    while (s->used != 56) st85_sha_update(s, &pad, 1);
    for (i = 0; i < 8; i++) lenb[i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xFFu);
    st85_sha_update(s, lenb, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(s->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(s->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(s->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(s->h[i]);
    }
}

/* --- growable text buffer ----------------------------------------------- */

typedef struct { char *buf; size_t len, cap; int oom; } st85_buf;

static void st85_put(st85_buf *b, const char *s, size_t n) {
    if (b->oom) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 128;
        char *nb;
        while (cap < b->len + n + 1) cap *= 2;
        nb = (char *)realloc(b->buf, cap);
        if (!nb) { b->oom = 1; return; }
        b->buf = nb;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void st85_puts(st85_buf *b, const char *s) { st85_put(b, s, strlen(s)); }
static void st85_putc(st85_buf *b, char c) { st85_put(b, &c, 1); }

/* --- Python literal renderers ------------------------------------------- */

/* json.dumps(str): ensure_ascii, so non-ASCII becomes \\uXXXX. */
static void st85_put_json_str(st85_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char esc[8];
    st85_putc(b, '"');
    while (*p) {
        unsigned char c = *p;
        if (c == '"')       { st85_puts(b, "\\\""); p++; }
        else if (c == '\\') { st85_puts(b, "\\\\"); p++; }
        else if (c == '\n') { st85_puts(b, "\\n"); p++; }
        else if (c == '\r') { st85_puts(b, "\\r"); p++; }
        else if (c == '\t') { st85_puts(b, "\\t"); p++; }
        else if (c == '\b') { st85_puts(b, "\\b"); p++; }
        else if (c == '\f') { st85_puts(b, "\\f"); p++; }
        else if (c < 0x20)  { snprintf(esc, sizeof(esc), "\\u%04x", c); st85_puts(b, esc); p++; }
        else if (c < 0x80)  { st85_putc(b, (char)c); p++; }
        else {
            unsigned long cp = 0;
            int extra = 0, k;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
            else { cp = c; extra = 0; }
            for (k = 1; k <= extra; k++) {
                if ((p[k] & 0xC0) != 0x80) { cp = c; extra = k - 1; break; }
                cp = (cp << 6) | (unsigned long)(p[k] & 0x3F);
            }
            if (cp > 0xFFFFUL) {
                unsigned long v = cp - 0x10000UL;
                snprintf(esc, sizeof(esc), "\\u%04lx", 0xD800UL + (v >> 10));
                st85_puts(b, esc);
                snprintf(esc, sizeof(esc), "\\u%04lx", 0xDC00UL + (v & 0x3FFUL));
                st85_puts(b, esc);
            } else {
                snprintf(esc, sizeof(esc), "\\u%04lx", cp);
                st85_puts(b, esc);
            }
            p += 1 + (size_t)extra;
        }
    }
    st85_putc(b, '"');
}

/* repr() of a JSON number: an int stays integral, a float keeps its shortest
 * round-trip form. json.dumps uses the same two renderings. */
static void st85_put_number(st85_buf *b, double v) {
    char tmp[64];
    int p;
    if (v == floor(v) && fabs(v) < 9.0e15) {
        snprintf(tmp, sizeof(tmp), "%lld", (long long)v);
        st85_puts(b, tmp);
        return;
    }
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) break;
    }
    st85_puts(b, tmp);
}

/* json.dumps(value, sort_keys=True, separators=(",", ":")) for JSON values. */
static void st85_canon(st85_buf *b, const json_t *v);

static void st85_canon_object(st85_buf *b, const json_t *v) {
    size_t n = v->c.count, i, j;
    char **keys = NULL;
    st85_putc(b, '{');
    if (n) {
        keys = (char **)malloc(n * sizeof(char *));
        if (!keys) { b->oom = 1; return; }
        for (i = 0; i < n; i++) keys[i] = v->c.keys[i];
        for (i = 1; i < n; i++) {           /* insertion sort: keys arrive tiny */
            char *k = keys[i];
            j = i;
            while (j > 0 && strcmp(keys[j - 1], k) > 0) { keys[j] = keys[j - 1]; j--; }
            keys[j] = k;
        }
    }
    for (i = 0; i < n; i++) {
        const json_t *item = NULL;
        size_t k;
        if (i) st85_putc(b, ',');
        st85_put_json_str(b, keys[i]);
        st85_putc(b, ':');
        for (k = 0; k < v->c.count; k++) {   /* duplicate keys keep the last value */
            if (!strcmp(v->c.keys[k], keys[i])) item = v->c.items[k];
        }
        st85_canon(b, item);
    }
    free(keys);
    st85_putc(b, '}');
}

static void st85_canon(st85_buf *b, const json_t *v) {
    size_t i;
    if (!v) { st85_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:   st85_puts(b, "null"); break;
    case JSON_BOOL:   st85_puts(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: st85_put_number(b, v->num_val); break;
    case JSON_STRING: st85_put_json_str(b, v->str_val); break;
    case JSON_ARRAY:
        st85_putc(b, '[');
        for (i = 0; i < v->c.count; i++) {
            if (i) st85_putc(b, ',');
            st85_canon(b, v->c.items[i]);
        }
        st85_putc(b, ']');
        break;
    case JSON_OBJECT: st85_canon_object(b, v); break;
    }
}

/* --- public API ---------------------------------------------------------- */

static void st85_set_empty_buckets(json_t *buckets, json_t *stats) {
    int i;
    for (i = 0; i < W20_STORE_EF85F3_BUCKET_COUNT; i++) {
        json_set(buckets, STORE_BUCKETS[i], json_object());
        json_set(stats, STORE_BUCKETS[i], json_number(0));
    }
}

json_t *w20_store_ef85f3_load_state(const char *text) {
    json_t *out = json_object(), *buckets = json_object(), *stats = json_object();
    json_t *doc = NULL;
    const char *p = text ? text : "";

    /* open(..., encoding="utf-8-sig"): the BOM never reaches json.loads. */
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF) p += 3;

    if (*p) doc = json_parse(p, NULL);
    else doc = json_object();            /* json.loads(text or "{}") */

    if (!doc) {
        /* The live constructor raises here; nothing is assigned. */
        st85_set_empty_buckets(buckets, stats);
        json_set(out, "buckets", buckets);
        json_set(out, "stats", stats);
        json_set(out, "parse_error", json_bool(1));
        return out;
    }
    if (doc->type != JSON_OBJECT) {      /* `if not isinstance(data, dict): return` */
        json_free(doc);
        st85_set_empty_buckets(buckets, stats);
        json_set(out, "buckets", buckets);
        json_set(out, "stats", stats);
        return out;
    }
    {
        int i;
        for (i = 0; i < W20_STORE_EF85F3_BUCKET_COUNT; i++) {
            json_t *v = json_obj_get(doc, STORE_BUCKETS[i]);
            json_t *bucket = json_object();
            size_t k;
            if (v && v->type == JSON_OBJECT) {
                for (k = 0; k < v->c.count; k++)
                    json_set(bucket, v->c.keys[k], json_copy(v->c.items[k]));
            } else if (v && v->type != JSON_NULL) {
                /* dict(value or {}): a falsy non-dict gives {}, and a truthy
                 * non-dict raises in Python -- the caller sees a parse error. */
                if (!((v->type == JSON_ARRAY && v->c.count == 0) ||
                      (v->type == JSON_STRING && (!v->str_val || !v->str_val[0])) ||
                      (v->type == JSON_NUMBER && v->num_val == 0.0) ||
                      (v->type == JSON_BOOL && !v->bool_val))) {
                    json_free(bucket);
                    json_free(doc);
                    st85_set_empty_buckets(buckets, stats);
                    json_set(out, "buckets", buckets);
                    json_set(out, "stats", stats);
                    json_set(out, "parse_error", json_bool(1));
                    return out;
                }
            }
            json_set(buckets, STORE_BUCKETS[i], bucket);
            json_set(stats, STORE_BUCKETS[i], json_number((double)bucket->c.count));
        }
    }
    json_free(doc);
    json_set(out, "buckets", buckets);
    json_set(out, "stats", stats);
    return out;
}

char *w20_store_ef85f3_receipt_key(const json_t *notification) {
    json_t *id = notification ? json_obj_get(notification, "id") : NULL;
    int truthy = 0;
    if (id) {
        switch (id->type) {
        case JSON_NULL: truthy = 0; break;
        case JSON_BOOL: truthy = id->bool_val ? 1 : 0; break;
        case JSON_NUMBER: truthy = id->num_val != 0.0; break;
        case JSON_STRING: truthy = id->str_val && id->str_val[0]; break;
        default: truthy = id->c.count != 0; break;
        }
    }
    if (truthy) {
        st85_buf b = {NULL, 0, 0, 0};
        char *out;
        st85_puts(&b, "id:");
        if (id->type == JSON_STRING) st85_puts(&b, id->str_val);
        else if (id->type == JSON_NUMBER) st85_put_number(&b, id->num_val);
        else if (id->type == JSON_BOOL) st85_puts(&b, "True");
        else { free(b.buf); return st85_dup(""); }
        out = b.oom ? NULL : b.buf;
        return out ? out : st85_dup("");
    }
    {
        st85_buf b = {NULL, 0, 0, 0};
        st85_sha sha;
        unsigned char digest[32];
        char hex[72];
        int i;
        st85_canon(&b, notification ? notification : json_null());
        if (b.oom) { free(b.buf); return NULL; }
        st85_sha_init(&sha);
        st85_sha_update(&sha, (const unsigned char *)b.buf, b.len);
        st85_sha_final(&sha, digest);
        memcpy(hex, "sha256:", 7);
        for (i = 0; i < 32; i++) snprintf(hex + 7 + i * 2, 3, "%02x", digest[i]);
        hex[71] = '\0';
        free(b.buf);
        return st85_dup(hex);
    }
}

const char *w20_store_ef85f3_id_field(const char *bucket) {
    if (!bucket) return NULL;
    if (!strcmp(bucket, "subscriptions")) return "subscription_id";
    if (!strcmp(bucket, "jobs")) return "job_id";
    if (!strcmp(bucket, "sink_records")) return "sink_key";
    return NULL;
}

json_t *w20_store_ef85f3_upsert(const char *bucket, const json_t *existing,
                                const json_t *payload, const char *key, const char *now) {
    const char *id_field = w20_store_ef85f3_id_field(bucket);
    json_t *merged = json_object();
    size_t i;
    if (existing && existing->type == JSON_OBJECT) {
        for (i = 0; i < existing->c.count; i++)
            json_set(merged, existing->c.keys[i], json_copy(existing->c.items[i]));
    }
    if (payload && payload->type == JSON_OBJECT) {
        for (i = 0; i < payload->c.count; i++)
            json_set(merged, payload->c.keys[i], json_copy(payload->c.items[i]));
    }
    if (!id_field) { json_free(merged); return NULL; }  /* no upsert binding for this bucket */
    json_set(merged, id_field, json_string(key ? key : ""));
    if (!json_obj_get(merged, "created_at")) {
        /* merged.setdefault("created_at", existing.get("created_at") or now) */
        json_t *have = existing ? json_obj_get(existing, "created_at") : NULL;
        int truthy = 0;
        if (have) {
            switch (have->type) {
            case JSON_NULL: truthy = 0; break;
            case JSON_BOOL: truthy = have->bool_val ? 1 : 0; break;
            case JSON_NUMBER: truthy = have->num_val != 0.0; break;
            case JSON_STRING: truthy = have->str_val && have->str_val[0]; break;
            default: truthy = have->c.count != 0; break;
            }
        }
        json_set(merged, "created_at", truthy ? json_copy(have) : json_string(now ? now : ""));
    }
    json_set(merged, "updated_at", json_string(now ? now : ""));
    return merged;
}

json_t *w20_store_ef85f3_record_receipt(const json_t *receipts, const char *key,
                                        const json_t *payload, const char *received_at,
                                        const char *now) {
    json_t *out = json_object(), *bucket = json_object(), *entry;
    size_t i;
    int seen;
    if (receipts && receipts->type == JSON_OBJECT) {
        for (i = 0; i < receipts->c.count; i++)
            json_set(bucket, receipts->c.keys[i], json_copy(receipts->c.items[i]));
    }
    seen = json_obj_get(bucket, key ? key : "") != NULL;
    if (!seen) {
        entry = json_object();
        json_set(entry, "received_at",
                 json_string((received_at && received_at[0]) ? received_at : (now ? now : "")));
        json_set(entry, "payload", payload ? json_copy(payload) : json_null());
        json_set(bucket, key ? key : "", entry);
    }
    json_set(out, "added", json_bool(seen ? 0 : 1));
    json_set(out, "receipts", bucket);
    return out;
}
