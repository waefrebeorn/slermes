/*
 * vocabulary.c -- real C11 port of the shared authority vocabulary of upstream
 * Hermes persistence/authority/vocabulary.py: the canonical admission
 * fingerprint (json.dumps(sort_keys, ensure_ascii=False, separators=(",", ":"))
 * + sha256) and the shared label sets every admission, worker and gateway
 * agrees on.
 *
 * PoP: w20_vocabulary_6f97e2_admission_fingerprint @ persistence/authority/vocabulary.py:admission_fingerprint
 * PoP: w20_vocabulary_6f97e2_queue_max @ persistence/authority/vocabulary.py:ADMISSION_QUEUE_MAX
 * PoP: w20_vocabulary_6f97e2_retry_horizon @ persistence/authority/vocabulary.py:RETRY_HORIZON_S
 * PoP: w20_vocabulary_6f97e2_admitted_request_conflict @ persistence/authority/vocabulary.py:ADMITTED_REQUEST_CONFLICT
 * PoP: w20_vocabulary_6f97e2_classify_label @ persistence/authority/vocabulary.py:LEGACY_LOCAL_SOURCES
 */
#include "hermes_json.h"
#include "noc_w20_vocabulary_6f97e2.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- small growable byte buffer ------------------------------------------- */

typedef struct {
    char  *ptr;
    size_t len;
    size_t cap;
    int    oom;
} v6f_buf;

static void v6f_buf_init(v6f_buf *b) {
    b->ptr = NULL;
    b->len = 0;
    b->cap = 0;
    b->oom = 0;
}

static void v6f_buf_reserve(v6f_buf *b, size_t extra) {
    size_t need = b->len + extra + 1;
    size_t cap;
    char *grown;
    if (b->oom || need <= b->cap) return;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    grown = (char *)realloc(b->ptr, cap);
    if (!grown) { b->oom = 1; return; }
    b->ptr = grown;
    b->cap = cap;
}

static void v6f_buf_putc(v6f_buf *b, char ch) {
    v6f_buf_reserve(b, 1);
    if (b->oom) return;
    b->ptr[b->len++] = ch;
    b->ptr[b->len] = '\0';
}

static void v6f_buf_puts(v6f_buf *b, const char *s) {
    size_t n = s ? strlen(s) : 0;
    v6f_buf_reserve(b, n);
    if (b->oom) return;
    if (n) memcpy(b->ptr + b->len, s, n);
    b->len += n;
    b->ptr[b->len] = '\0';
}

static void v6f_buf_free(v6f_buf *b) {
    free(b->ptr);
    b->ptr = NULL;
    b->len = b->cap = 0;
}

/* --- Python json.dumps scalar forms --------------------------------------- */

/* json.dumps(float) uses repr(); the shortest round-tripping form is what
 * repr() prints, with a float marker whenever the text would read as an int. */
static void v6f_num_text(char *dst, size_t cap, double d) {
    int precision;
    if (isfinite(d) && floor(d) == d && fabs(d) <= 9007199254740992.0) {
        snprintf(dst, cap, "%lld", (long long)d);
        return;
    }
    dst[0] = '\0';
    for (precision = 1; precision <= 17; precision++) {
        snprintf(dst, cap, "%.*g", precision, d);
        if (strtod(dst, NULL) == d) break;
    }
    if (!strpbrk(dst, ".eEnN")) {
        size_t n = strlen(dst);
        if (n + 3 <= cap) { dst[n] = '.'; dst[n + 1] = '0'; dst[n + 2] = '\0'; }
    }
}

static void v6f_enc_string(v6f_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    v6f_buf_putc(b, '"');
    for (; *p; p++) {
        unsigned char ch = *p;
        switch (ch) {
        case '"':  v6f_buf_puts(b, "\\\""); break;
        case '\\': v6f_buf_puts(b, "\\\\"); break;
        case '\n': v6f_buf_puts(b, "\\n");  break;
        case '\r': v6f_buf_puts(b, "\\r");  break;
        case '\t': v6f_buf_puts(b, "\\t");  break;
        case '\b': v6f_buf_puts(b, "\\b");  break;
        case '\f': v6f_buf_puts(b, "\\f");  break;
        default:
            if (ch < 0x20) {
                char tmp[8];
                snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)ch);
                v6f_buf_puts(b, tmp);
            } else {
                v6f_buf_putc(b, (char)ch);
            }
        }
    }
    v6f_buf_putc(b, '"');
}

static int v6f_key_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* One json.dumps(..., sort_keys=True, separators=(",", ":"), ensure_ascii=False) pass. */
static void v6f_enc_value(v6f_buf *b, const json_t *v) {
    size_t i, n;
    if (!v) { v6f_buf_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:   v6f_buf_puts(b, "null"); break;
    case JSON_BOOL:   v6f_buf_puts(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: {
        char tmp[40];
        v6f_num_text(tmp, sizeof(tmp), v->num_val);
        v6f_buf_puts(b, tmp);
        break;
    }
    case JSON_STRING:
        v6f_enc_string(b, v->str_val);
        break;
    case JSON_ARRAY:
        v6f_buf_putc(b, '[');
        n = v->c.count;
        for (i = 0; i < n; i++) {
            if (i) v6f_buf_putc(b, ',');
            v6f_enc_value(b, v->c.items[i]);
        }
        v6f_buf_putc(b, ']');
        break;
    case JSON_OBJECT: {
        const char **keys;
        n = v->c.count;
        keys = n ? (const char **)malloc(n * sizeof(*keys)) : NULL;
        if (n && !keys) { b->oom = 1; return; }
        for (i = 0; i < n; i++) keys[i] = v->c.keys[i] ? v->c.keys[i] : "";
        qsort(keys, n, sizeof(*keys), v6f_key_cmp);
        v6f_buf_putc(b, '{');
        for (i = 0; i < n; i++) {
            json_t *item;
            if (i) v6f_buf_putc(b, ',');
            v6f_enc_string(b, keys[i]);
            v6f_buf_putc(b, ':');
            item = json_obj_get(v, keys[i]);
            v6f_enc_value(b, item);
        }
        v6f_buf_putc(b, '}');
        free(keys);
        break;
    }
    default:
        v6f_buf_puts(b, "null");
        break;
    }
}

/* --- SHA-256 (FIPS 180-4), self-contained -------------------------------- */

typedef struct {
    unsigned int   state[8];
    unsigned long long total;
    unsigned char  block[64];
    size_t         used;
} v6f_sha256;

static const unsigned int V6F_K[64] = {
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

static unsigned int v6f_rotr(unsigned int x, unsigned int n) {
    return (x >> n) | (x << (32u - n));
}

static void v6f_sha256_block(v6f_sha256 *ctx, const unsigned char *p) {
    unsigned int w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((unsigned int)p[i * 4] << 24) | ((unsigned int)p[i * 4 + 1] << 16) |
               ((unsigned int)p[i * 4 + 2] << 8) | (unsigned int)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        unsigned int rot15a = v6f_rotr(w[i - 15], 7), rot15b = v6f_rotr(w[i - 15], 18);
        unsigned int rot2a = v6f_rotr(w[i - 2], 17), rot2b = v6f_rotr(w[i - 2], 19);
        unsigned int s0 = rot15a ^ rot15b ^ (w[i - 15] >> 3);
        unsigned int s1 = rot2a ^ rot2b ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; i++) {
        unsigned int s1 = v6f_rotr(e, 6) ^ v6f_rotr(e, 11) ^ v6f_rotr(e, 25);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int s0 = v6f_rotr(a, 2) ^ v6f_rotr(a, 13) ^ v6f_rotr(a, 22);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + s1 + ch + V6F_K[i] + w[i];
        t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void v6f_sha256_init(v6f_sha256 *ctx) {
    ctx->state[0] = 0x6a09e667u; ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u; ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu; ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu; ctx->state[7] = 0x5be0cd19u;
    ctx->total = 0;
    ctx->used = 0;
}

static void v6f_sha256_update(v6f_sha256 *ctx, const unsigned char *p, size_t n) {
    ctx->total += (unsigned long long)n;
    while (n) {
        size_t room = 64 - ctx->used;
        size_t take = n < room ? n : room;
        memcpy(ctx->block + ctx->used, p, take);
        ctx->used += take;
        p += take;
        n -= take;
        if (ctx->used == 64) {
            v6f_sha256_block(ctx, ctx->block);
            ctx->used = 0;
        }
    }
}

static void v6f_sha256_final(v6f_sha256 *ctx, unsigned char out[32]) {
    unsigned long long bits = ctx->total * 8u;
    int i;
    ctx->block[ctx->used++] = 0x80;
    if (ctx->used > 56) {
        while (ctx->used < 64) ctx->block[ctx->used++] = 0;
        v6f_sha256_block(ctx, ctx->block);
        ctx->used = 0;
    }
    while (ctx->used < 56) ctx->block[ctx->used++] = 0;
    for (i = 0; i < 8; i++) ctx->block[56 + i] = (unsigned char)(bits >> ((7 - i) * 8));
    v6f_sha256_block(ctx, ctx->block);
    for (i = 0; i < 8; i++) {
        out[i * 4]     = (unsigned char)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(ctx->state[i]);
    }
    ctx->used = 0;
}

static void v6f_sha256_hex(const unsigned char *data, size_t n, char out[65]) {
    static const char HEX[] = "0123456789abcdef";
    v6f_sha256 ctx;
    unsigned char sum[32];
    int i;
    v6f_sha256_init(&ctx);
    v6f_sha256_update(&ctx, data, n);
    v6f_sha256_final(&ctx, sum);
    for (i = 0; i < 32; i++) {
        out[i * 2]     = HEX[sum[i] >> 4];
        out[i * 2 + 1] = HEX[sum[i] & 0x0f];
    }
    out[64] = '\0';
}

/* --- exported API -------------------------------------------------------- */

struct json_t *w20_vocabulary_6f97e2_admission_fingerprint(const char *canonical_target,
                                                           const char *payload_json) {
    json_t *payload = NULL;
    json_t *wire;
    v6f_buf text;
    char digest[65];

    if (payload_json && payload_json[0]) payload = json_parse(payload_json, NULL);
    wire = json_object();
    json_set(wire, "target", json_string(canonical_target ? canonical_target : ""));
    json_set(wire, "payload", payload ? payload : json_null());

    v6f_buf_init(&text);
    v6f_enc_value(&text, wire);
    if (text.oom || !text.ptr) {
        v6f_buf_free(&text);
        json_free(wire);
        return json_null();
    }
    v6f_sha256_hex((const unsigned char *)text.ptr, text.len, digest);
    v6f_buf_free(&text);
    json_free(wire);
    return json_string(digest);
}

struct json_t *w20_vocabulary_6f97e2_queue_max(void) {
    return json_number(32.0);
}

struct json_t *w20_vocabulary_6f97e2_retry_horizon(void) {
    return json_number(604800.0);
}

struct json_t *w20_vocabulary_6f97e2_admitted_request_conflict(void) {
    json_t *out = json_object();
    json_set(out, "field", json_string("request_id"));
    json_set(out, "detail", json_string("this request id was already admitted with another payload"));
    return out;
}

static const char *const V6F_LEGACY_LOCAL[] = {
    "cli", "tui", "gui", "desktop", "oneshot", "tool", "acp", NULL
};

static const char *const V6F_NOT_CLIENT[] = {
    "cron", "subagent", "kanban", "bot_room", "a2a", "webhook", "api_server",
    "import", "recovered", "curator", "batch", "homeassistant", "webui", "local",
    "unknown", NULL
};

static int v6f_in_list(const char *value, const char *const *list) {
    int i;
    if (!value) return 0;
    for (i = 0; list[i]; i++) {
        if (!strcmp(value, list[i])) return 1;
    }
    return 0;
}

struct json_t *w20_vocabulary_6f97e2_classify_label(const char *label) {
    json_t *out = json_object();
    const char *creation = NULL;
    int legacy_local = v6f_in_list(label, V6F_LEGACY_LOCAL);
    int not_client = v6f_in_list(label, V6F_NOT_CLIENT);

    if (label && !strcmp(label, "desktop")) creation = "gui";
    json_set(out, "legacy_local", json_bool(legacy_local));
    json_set(out, "legacy_creation_source", creation ? json_string(creation) : json_null());
    json_set(out, "not_client", json_bool(not_client));
    return out;
}
