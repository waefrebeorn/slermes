/*
 * usage_anchor.c — faithful C11 port of upstream Hermes agent/usage_anchor.py.
 *
 * PoP: usageanchor_fingerprint @ agent/usage_anchor.py:message_fingerprint
 * PoP: usageanchor_capture @ agent/usage_anchor.py:capture_usage_anchor
 * PoP: usageanchor_anchor_matches @ agent/usage_anchor.py:_anchor_matches
 * PoP: usageanchor_serialize @ agent/usage_anchor.py:_serialize
 * PoP: usageanchor_context_tokens @ agent/usage_anchor.py:anchored_context_tokens
 *
 * Fingerprints are sha256 over json.dumps(payload, sort_keys=True,
 * default=str, ensure_ascii=True, separators=(",", ":")); we implement that
 * exact canonical serializer (integer fixtures only) plus SHA-256 in-tree.
 */
#include "noc_w6_usageanchor_c3af05.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

/* ---------------- dynamic string builder ---------------- */
typedef struct { char *d; size_t n, cap; } SB;
static void sb_init(SB *b) { b->cap = 128; b->n = 0; b->d = (char *)malloc(b->cap); if (b->d) b->d[0] = 0; }
static void sb_grow(SB *b, size_t extra) {
    if (!b->d) return;
    if (b->n + extra + 1 > b->cap) {
        while (b->n + extra + 1 > b->cap) b->cap *= 2;
        b->d = (char *)realloc(b->d, b->cap);
    }
}
static void sb_putn(SB *b, const char *s, size_t n) { sb_grow(b, n); memcpy(b->d + b->n, s, n); b->n += n; b->d[b->n] = 0; }
static void sb_put(SB *b, const char *s) { if (s) sb_putn(b, s, strlen(s)); }
static void sb_ch(SB *b, char c) { sb_grow(b, 1); b->d[b->n++] = c; b->d[b->n] = 0; }
static void sb_uhex(SB *b, unsigned int v, int width) {
    const char *h = "0123456789abcdef";
    char tmp[16]; int n = 0;
    if (width) {
        for (int i = width - 1; i >= 0; i--) tmp[n++] = h[(v >> (i * 4)) & 0xf];
    } else {
        char rev[16]; int r = 0;
        if (!v) rev[r++] = '0';
        while (v) { rev[r++] = h[v & 0xf]; v >>= 4; }
        while (r) tmp[n++] = rev[--r];
    }
    sb_putn(b, tmp, (size_t)n);
}
static void sb_free(SB *b) { free(b->d); b->d = NULL; }

/* ---------------- SHA-256 ---------------- */
typedef struct { uint32_t h[8]; uint64_t bits; unsigned char buf[64]; size_t bn; } UA_SHA;
static const uint32_t UA_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static uint32_t ua_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void ua_sha_block(UA_SHA *s, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4*i] << 24) | ((uint32_t)p[4*i+1] << 16) |
               ((uint32_t)p[4*i+2] << 8) | (uint32_t)p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ua_rotr(w[i-15],7) ^ ua_rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ua_rotr(w[i-2],17) ^ ua_rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ua_rotr(e,6)^ua_rotr(e,11)^ua_rotr(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + UA_K[i] + w[i];
        uint32_t S0 = ua_rotr(a,2)^ua_rotr(a,13)^ua_rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d;
    s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}
static void ua_sha_init(UA_SHA *s) {
    s->h[0]=0x6a09e667; s->h[1]=0xbb67ae85; s->h[2]=0x3c6ef372; s->h[3]=0xa54ff53a;
    s->h[4]=0x510e527f; s->h[5]=0x9b05688c; s->h[6]=0x1f83d9ab; s->h[7]=0x5be0cd19;
    s->bits=0; s->bn=0;
}
static void ua_sha_update(UA_SHA *s, const unsigned char *d, size_t n) {
    s->bits += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - s->bn;
        if (take > n) take = n;
        memcpy(s->buf + s->bn, d, take);
        s->bn += take; d += take; n -= take;
        if (s->bn == 64) { ua_sha_block(s, s->buf); s->bn = 0; }
    }
}
static void ua_sha_final(UA_SHA *s, unsigned char out[32]) {
    uint64_t bits = s->bits;
    unsigned char pad = 0x80;
    ua_sha_update(s, &pad, 1);
    unsigned char zero = 0;
    while (s->bn != 56) ua_sha_update(s, &zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; i++) len[i] = (unsigned char)(bits >> (56 - 8*i));
    /* append length without disturbing s->bits accounting */
    for (int i = 0; i < 8; i++) {
        s->buf[s->bn++] = len[i];
        if (s->bn == 64) { ua_sha_block(s, s->buf); s->bn = 0; }
    }
    for (int i = 0; i < 8; i++) {
        out[4*i] = (unsigned char)(s->h[i] >> 24);
        out[4*i+1] = (unsigned char)(s->h[i] >> 16);
        out[4*i+2] = (unsigned char)(s->h[i] >> 8);
        out[4*i+3] = (unsigned char)(s->h[i]);
    }
}
static char *ua_sha256_hex(const char *data, size_t n) {
    UA_SHA s; unsigned char dig[32];
    ua_sha_init(&s);
    ua_sha_update(&s, (const unsigned char *)data, n);
    ua_sha_final(&s, dig);
    char *out = (char *)malloc(65);
    if (out) { for (int i = 0; i < 32; i++) { out[2*i] = "0123456789abcdef"[dig[i]>>4]; out[2*i+1] = "0123456789abcdef"[dig[i]&0xf]; } out[64]=0; }
    return out;
}

/* ---------------- Python-compatible compact JSON dump ---------------- */
static int ua_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static void ua_escape_str(SB *sb, const char *s) {
    if (!s) s = "";
    sb_ch(sb, '"');
    for (size_t i = 0; s[i]; ) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') { sb_put(sb, "\\\""); i++; continue; }
        if (c == '\\') { sb_put(sb, "\\\\"); i++; continue; }
        if (c == '\b') { sb_put(sb, "\\b"); i++; continue; }
        if (c == '\f') { sb_put(sb, "\\f"); i++; continue; }
        if (c == '\n') { sb_put(sb, "\\n"); i++; continue; }
        if (c == '\r') { sb_put(sb, "\\r"); i++; continue; }
        if (c == '\t') { sb_put(sb, "\\t"); i++; continue; }
        if (c < 0x20) { sb_put(sb, "\\u"); sb_uhex(sb, c, 4); i++; continue; }
        if (c < 0x80) { sb_ch(sb, (char)c); i++; continue; }
        uint32_t cp; size_t adv;
        if ((c & 0xE0) == 0xC0) { cp = ((uint32_t)(c & 0x1f) << 6) | (unsigned char)(s[i+1] & 0x3f); adv = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = ((uint32_t)(c & 0x0f) << 12) | ((uint32_t)(unsigned char)(s[i+1] & 0x3f) << 6) | (unsigned char)(s[i+2] & 0x3f); adv = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(unsigned char)(s[i+1] & 0x3f) << 12) | ((uint32_t)(unsigned char)(s[i+2] & 0x3f) << 6) | (unsigned char)(s[i+3] & 0x3f); adv = 4; }
        else { cp = c; adv = 1; }
        if (cp <= 0xFFFF) { sb_put(sb, "\\u"); sb_uhex(sb, cp, 4); }
        else {
            uint32_t v = cp - 0x10000;
            sb_put(sb, "\\u"); sb_uhex(sb, 0xD800 + (v >> 10), 4);
            sb_put(sb, "\\u"); sb_uhex(sb, 0xDC00 + (v & 0x3FF), 4);
        }
        i += adv;
    }
    sb_ch(sb, '"');
}
static void ua_dump(const json_t *v, int sort_keys, SB *sb) {
    if (!v || v->type == JSON_NULL) { sb_put(sb, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: sb_put(sb, v->bool_val ? "true" : "false"); return;
        case JSON_STRING: ua_escape_str(sb, v->str_val); return;
        case JSON_NUMBER: {
            char tmp[64];
            double d = v->num_val;
            if (d == floor(d) && d > -1e15 && d < 1e15) snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
            else snprintf(tmp, sizeof(tmp), "%.17g", d);
            sb_put(sb, tmp);
            return;
        }
        case JSON_ARRAY: {
            sb_ch(sb, '[');
            for (size_t i = 0; i < json_len(v); i++) {
                if (i) sb_ch(sb, ',');
                ua_dump(json_get(v, i), sort_keys, sb);
            }
            sb_ch(sb, ']');
            return;
        }
        case JSON_OBJECT: {
            sb_ch(sb, '{');
            size_t cnt = json_object_size(v);
            if (sort_keys && cnt > 1) {
                const char **keys = (const char **)malloc(cnt * sizeof(char *));
                for (size_t i = 0; i < cnt; i++) keys[i] = json_object_get_key_at(v, i);
                qsort(keys, cnt, sizeof(char *), ua_cmp_key);
                for (size_t i = 0; i < cnt; i++) {
                    if (i) sb_ch(sb, ',');
                    ua_escape_str(sb, keys[i]); sb_ch(sb, ':');
                    ua_dump(json_obj_get(v, keys[i]), 1, sb);
                }
                free(keys);
            } else {
                for (size_t i = 0; i < cnt; i++) {
                    if (i) sb_ch(sb, ',');
                    ua_escape_str(sb, json_object_get_key_at(v, i)); sb_ch(sb, ':');
                    ua_dump(json_object_get_at(v, i), sort_keys, sb);
                }
            }
            sb_ch(sb, '}');
            return;
        }
        default: sb_put(sb, "null");
    }
}

/* ---------------- int() coercion / equality ---------------- */
static int ua_to_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) { *out = 0; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) { *out = 0; return 1; }
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = x; return 1;
    }
    return 0;
}
static int ua_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val, b->str_val);
        default: {
            SB x, y; sb_init(&x); sb_init(&y);
            ua_dump(a, 0, &x); ua_dump(b, 0, &y);
            int eq = !strcmp(x.d, y.d);
            sb_free(&x); sb_free(&y);
            return eq;
        }
    }
}

/* ---------------- exports ---------------- */
static const char *UA_KEYS[] = {"role", "content", "api_content", "tool_call_id", "tool_calls"};
#define UA_NKEYS 5

json_t *usageanchor_fingerprint(const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return json_null();
    json_t *payload = json_object();
    for (int i = 0; i < UA_NKEYS; i++) {
        json_t *v = json_obj_get(msg, UA_KEYS[i]);
        if (v && v->type != JSON_NULL) json_set(payload, UA_KEYS[i], json_copy(v));
    }
    SB sb; sb_init(&sb);
    ua_dump(payload, 1, &sb);
    char *hex = ua_sha256_hex(sb.d ? sb.d : "", sb.n);
    sb_free(&sb); json_free(payload);
    json_t *r = hex ? json_string(hex) : json_null();
    free(hex);
    return r;
}

static json_t *ua_prefix_fp(const json_t *messages, long long base_count) {
    if (base_count <= 0 || !messages || messages->type != JSON_ARRAY ||
        (long long)json_len(messages) < base_count) return json_null();
    SB sb; sb_init(&sb);
    sb_ch(&sb, '[');
    for (long long i = 0; i < base_count; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        json_t *fp = usageanchor_fingerprint(msg);
        if (!fp || fp->type != JSON_STRING) { sb_free(&sb); json_free(fp); return json_null(); }
        if (i) sb_ch(&sb, ',');
        sb_ch(&sb, '[');
        json_t *role = (msg && msg->type == JSON_OBJECT) ? json_obj_get(msg, "role") : NULL;
        ua_dump(role, 0, &sb);
        sb_ch(&sb, ',');
        ua_dump(fp, 0, &sb);
        sb_ch(&sb, ']');
        json_free(fp);
    }
    sb_ch(&sb, ']');
    char *hex = ua_sha256_hex(sb.d ? sb.d : "", sb.n);
    sb_free(&sb);
    json_t *r = hex ? json_string(hex) : json_null();
    free(hex);
    return r;
}

json_t *usageanchor_capture(const json_t *prompt_tokens, const json_t *completion_tokens,
                            const json_t *messages) {
    long long pt, ct;
    if (!ua_to_int(prompt_tokens, &pt) || !ua_to_int(completion_tokens, &ct)) return json_null();
    if (pt <= 0 || !messages || messages->type != JSON_ARRAY || json_len(messages) == 0)
        return json_null();
    json_t *last = json_get(messages, json_len(messages) - 1);
    json_t *anchor = json_object();
    json_set(anchor, "prompt_tokens", json_number((double)pt));
    json_set(anchor, "completion_tokens", json_number((double)(ct > 0 ? ct : 0)));
    json_set(anchor, "base_count", json_number((double)json_len(messages)));
    json_t *role = (last && last->type == JSON_OBJECT) ? json_obj_get(last, "role") : NULL;
    json_set(anchor, "base_last_role", role ? json_copy(role) : json_null());
    json_t *fp = usageanchor_fingerprint(last);
    json_set(anchor, "base_last_fp", fp);
    json_t *prefix = ua_prefix_fp(messages, (long long)json_len(messages));
    json_set(anchor, "base_prefix_fp", prefix);
    return anchor;
}

static int ua_anchor_matches(const json_t *messages, const json_t *anchor) {
    if (!anchor || anchor->type != JSON_OBJECT || !messages || messages->type != JSON_ARRAY) return 0;
    long long bc;
    if (!ua_to_int(json_obj_get(anchor, "base_count"), &bc)) return 0;
    if (bc <= 0 || (long long)json_len(messages) < bc) return 0;
    json_t *base = json_get(messages, (size_t)(bc - 1));
    if (!base || base->type != JSON_OBJECT) return 0;
    if (!ua_json_eq(json_obj_get(base, "role"), json_obj_get(anchor, "base_last_role"))) return 0;
    json_t *fp = json_obj_get(anchor, "base_last_fp");
    if (!fp || fp->type != JSON_STRING || !fp->str_val[0]) return 0;
    json_t *live_fp = usageanchor_fingerprint(base);
    int ok = ua_json_eq(live_fp, fp);
    json_free(live_fp);
    if (!ok) return 0;
    json_t *pfp = json_obj_get(anchor, "base_prefix_fp");
    if (!pfp || pfp->type != JSON_STRING || !pfp->str_val[0]) return 0;
    json_t *live_pfp = ua_prefix_fp(messages, bc);
    ok = ua_json_eq(live_pfp, pfp);
    json_free(live_pfp);
    return ok;
}
json_t *usageanchor_anchor_matches(const json_t *messages, const json_t *anchor) {
    return json_bool(ua_anchor_matches(messages, anchor));
}

json_t *usageanchor_serialize(const json_t *anchor) {
    if (!anchor || anchor->type != JSON_OBJECT) return json_null();
    long long pt, ct, bc;
    if (!ua_to_int(json_obj_get(anchor, "prompt_tokens"), &pt) ||
        !ua_to_int(json_obj_get(anchor, "completion_tokens"), &ct) ||
        !ua_to_int(json_obj_get(anchor, "base_count"), &bc)) return json_null();
    json_t *fp = json_obj_get(anchor, "base_last_fp");
    json_t *role = json_obj_get(anchor, "base_last_role");
    json_t *pfp = json_obj_get(anchor, "base_prefix_fp");
    if (pt <= 0 || bc <= 0 || !fp || fp->type != JSON_STRING || !fp->str_val[0] ||
        !pfp || pfp->type != JSON_STRING || !pfp->str_val[0]) return json_null();
    json_t *res = json_object();
    json_set(res, "prompt_tokens", json_number((double)pt));
    json_set(res, "completion_tokens", json_number((double)(ct > 0 ? ct : 0)));
    json_set(res, "base_count", json_number((double)bc));
    json_set(res, "base_last_role", (role && role->type == JSON_STRING) ? json_copy(role) : json_null());
    json_set(res, "base_last_fp", json_copy(fp));
    json_set(res, "base_prefix_fp", json_copy(pfp));
    return res;
}

json_t *usageanchor_context_tokens(const json_t *messages, const json_t *anchor,
                                   double delta_estimate) {
    if (!ua_anchor_matches(messages, anchor)) return json_null();
    long long pt, ct, bc;
    ua_to_int(json_obj_get(anchor, "prompt_tokens"), &pt);
    ua_to_int(json_obj_get(anchor, "completion_tokens"), &ct);
    ua_to_int(json_obj_get(anchor, "base_count"), &bc);
    long long total = pt + (ct > 0 ? ct : 0);
    size_t n = json_len(messages);
    size_t start = (size_t)bc;
    if (start < n) {
        json_t *first = json_get(messages, start);
        if (first && first->type == JSON_OBJECT) {
            json_t *r = json_obj_get(first, "role");
            if (r && r->type == JSON_STRING && !strcmp(r->str_val, "assistant")) start++;
        }
    }
    if (start < n) total += (long long)delta_estimate;
    return json_number((double)total);
}
