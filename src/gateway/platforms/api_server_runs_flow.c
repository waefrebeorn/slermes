/*
 * api_server_runs_flow.c — run-lifecycle FLOW cores of gateway/platforms/api_server_runs.py.
 *
 * Decision-sequence port: every live I/O dependency enters through asrun_seams_t
 * (see include/api_server_runs_flow.h). The cores below carry the same branch
 * order, truthiness, short-circuit evaluation, dict key order and error mapping
 * as the live Python; tests/oracle_api_server_runs_flow.py drives the LIVE
 * functions with identical scripted seams and compares the ordered trace plus
 * return values and mutated state.
 *
 * Self-contained: libc + libjson only (sha256 and the Python-str/json.dumps
 * formatting helpers are implemented here so the oracle gate links just
 * lib/libjson/json.o). The harness #includes this file directly.
 *
 * Python semantics carried explicitly:
 *  - truthiness: None/False/0/""/[]/{} are falsy (asrun_truthy).
 *  - `completed is not False` is IDENTITY: JSON number 0 is not bool False.
 *  - dict.update/setdefault preserve insertion position of existing keys;
 *    libjson json_set replaces in place, matching Python dict order.
 *  - `x or y` chains evaluate left to right and pick the first truthy value.
 *  - str()/int()/float() coercions follow Python (None -> "None", True ->
 *    "True", integral floats keep the ".0" in str(), int() truncates).
 *  - integral JSON numbers str/dump as ints (json_t carries no int/float
 *    identity — a JSON 1.0 literal reads back as 1; same convention as
 *    api_server_flow.c:apisf_make_request_fingerprint).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "hermes_json.h"
#include "api_server_runs_flow.h"

/* ── Trace recorder ─────────────────────────────────────────────────────── */

void asrun_trace_add(asrun_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

void asrun_trace_free(asrun_trace_t *t) {
    if (t && t->events) { json_free(t->events); t->events = NULL; }
}

/* ── Python truthiness / coercions ──────────────────────────────────────── */

static bool asrun_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* Shortest round-tripping decimal for a double, formatted like Python
 * repr(float): fixed notation for decimal exponents in [-4, 16), scientific
 * otherwise, at least one fractional digit in fixed notation, two-digit
 * exponent. Integral values keep ".0" (str(100.0) == "100.0"). */
static void asrun_float_repr(double v, char *out, size_t cap) {
    if (isnan(v)) { snprintf(out, cap, "nan"); return; }
    if (isinf(v)) { snprintf(out, cap, v < 0 ? "-inf" : "inf"); return; }
    if (v == 0.0) { snprintf(out, cap, signbit(v) ? "-0.0" : "0.0"); return; }
    bool neg = v < 0;
    double av = neg ? -v : v;
    char buf[64];
    int p;
    for (p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*e", p - 1, av);
        if (strtod(buf, NULL) == av) break;
    }
    if (p > 17) p = 17;
    /* buf is d.dddde±XX with p significant digits. */
    char digits[32];
    size_t nd = 0;
    digits[nd++] = buf[0];
    for (int i = 2; buf[i] && buf[i] != 'e' && buf[i] != 'E'; i++)
        digits[nd++] = buf[i];
    digits[nd] = '\0';
    while (nd > 1 && digits[nd - 1] == '0') digits[--nd] = '\0';
    int exp = atoi(strchr(buf, 'e') + 1);
    char body[128];
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t ip = (size_t)exp + 1;
            size_t o = 0;
            for (size_t i = 0; i < ip; i++)
                body[o++] = i < nd ? digits[i] : '0';
            body[o++] = '.';
            if (nd > ip) {
                for (size_t i = ip; i < nd; i++) body[o++] = digits[i];
            } else {
                body[o++] = '0';
            }
            body[o] = '\0';
        } else {
            size_t o = 0;
            body[o++] = '0'; body[o++] = '.';
            for (int i = 0; i < -exp - 1; i++) body[o++] = '0';
            for (size_t i = 0; i < nd; i++) body[o++] = digits[i];
            body[o] = '\0';
        }
    } else {
        char frac[32];
        size_t o = 0;
        frac[o++] = digits[0];
        if (nd > 1) {
            frac[o++] = '.';
            for (size_t i = 1; i < nd; i++) frac[o++] = digits[i];
        }
        frac[o] = '\0';
        snprintf(body, sizeof(body), "%se%c%02d", frac, exp < 0 ? '-' : '+',
                 exp < 0 ? -exp : exp);
    }
    snprintf(out, cap, "%s%s", neg ? "-" : "", body);
}

/* malloc'd Python str() of a JSON scalar. Objects/arrays are not exercised
 * by the fixtures (live str() of a container is its repr); they read as "". */
static char *asrun_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.2233720368547758e18) {
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
                return strdup(buf);
            }
            asrun_float_repr(v->num_val, buf, sizeof(buf));
            return strdup(buf);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return strdup("");
    }
}

/* Python float() of a JSON scalar; *ok false when not convertible. */
static double asrun_py_float(const json_t *v, bool *ok) {
    *ok = true;
    if (!v) { *ok = false; return 0.0; }
    switch (v->type) {
        case JSON_NUMBER: return v->num_val;
        case JSON_BOOL: return v->bool_val ? 1.0 : 0.0;
        case JSON_STRING: {
            if (!v->str_val) { *ok = false; return 0.0; }
            char *end = NULL;
            double d = strtod(v->str_val, &end);
            if (end == v->str_val) { *ok = false; return 0.0; }
            while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
            if (*end != '\0') { *ok = false; return 0.0; }
            return d;
        }
        default: *ok = false; return 0.0;
    }
}

/* Python int() of a truthy JSON scalar (callers apply the `or 0` first). */
static long asrun_py_int(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NUMBER: return (long)v->num_val;   /* truncates toward zero */
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_STRING: return v->str_val ? strtol(v->str_val, NULL, 10) : 0;
        default: return 0;
    }
}

/* Python type(v).__name__ for a JSON scalar/container. Integral JSON numbers
 * read as int (json_t carries no int/float identity); a fixture written as
 * "5.0" would be a float in live json.loads, so the .get-AttributeError domain
 * excludes integral-float literals (see the port header). */
static const char *asrun_py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.2233720368547758e18)
                       ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* ── sha256 (FIPS 180-4), self-contained so the gate links only libjson ─── */

static const uint32_t asrun_sha_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static uint32_t asrun_rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

static void asrun_sha_block(uint32_t h[8], const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = asrun_rotr(w[i - 15], 7) ^ asrun_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = asrun_rotr(w[i - 2], 17) ^ asrun_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = asrun_rotr(e, 6) ^ asrun_rotr(e, 11) ^ asrun_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + asrun_sha_k[i] + w[i];
        uint32_t S0 = asrun_rotr(a, 2) ^ asrun_rotr(a, 13) ^ asrun_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

/* hex is 65 bytes (64 hex digits + NUL). */
static void asrun_sha256_hex(const unsigned char *data, size_t len, char *hex) {
    uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    size_t full = len / 64;
    for (size_t i = 0; i < full; i++) asrun_sha_block(h, data + i * 64);
    unsigned char tail[128];
    size_t rem = len % 64;
    memcpy(tail, data + full * 64, rem);
    tail[rem] = 0x80;
    size_t pad_to = (rem < 56) ? 56 : 120;
    memset(tail + rem + 1, 0, pad_to - rem - 1);
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) tail[pad_to + i] = (unsigned char)(bits >> (56 - i * 8));
    asrun_sha_block(h, tail);
    if (pad_to == 120) asrun_sha_block(h, tail + 64);
    for (int i = 0; i < 8; i++) snprintf(hex + i * 8, 9, "%08x", h[i]);
    hex[64] = '\0';
}

/* ── Python json.dumps(result, ensure_ascii=False, default=str) ─────────── */

typedef struct { char *b; size_t len, cap; } asrun_sb;

static void asrun_sb_init(asrun_sb *s) { s->cap = 128; s->len = 0; s->b = malloc(s->cap); s->b[0] = '\0'; }
static void asrun_sb_need(asrun_sb *s, size_t extra) {
    if (s->len + extra + 1 > s->cap) {
        while (s->len + extra + 1 > s->cap) s->cap *= 2;
        s->b = realloc(s->b, s->cap);
    }
}
static void asrun_sb_putc(asrun_sb *s, char c) { asrun_sb_need(s, 1); s->b[s->len++] = c; s->b[s->len] = '\0'; }
static void asrun_sb_puts(asrun_sb *s, const char *t) {
    size_t n = strlen(t);
    asrun_sb_need(s, n);
    memcpy(s->b + s->len, t, n);
    s->len += n;
    s->b[s->len] = '\0';
}

/* Python JSON string escaping with ensure_ascii=False: only '"', '\\' and
 * C0 controls are escaped; UTF-8 passes through verbatim. */
static void asrun_sb_json_string(asrun_sb *s, const char *t) {
    asrun_sb_putc(s, '"');
    for (const unsigned char *p = (const unsigned char *)(t ? t : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': asrun_sb_puts(s, "\\\""); break;
            case '\\': asrun_sb_puts(s, "\\\\"); break;
            case '\b': asrun_sb_puts(s, "\\b"); break;
            case '\f': asrun_sb_puts(s, "\\f"); break;
            case '\n': asrun_sb_puts(s, "\\n"); break;
            case '\r': asrun_sb_puts(s, "\\r"); break;
            case '\t': asrun_sb_puts(s, "\\t"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)c);
                    asrun_sb_puts(s, esc);
                } else {
                    asrun_sb_putc(s, (char)c);
                }
        }
    }
    asrun_sb_putc(s, '"');
}

static void asrun_sb_dump(asrun_sb *s, const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) { asrun_sb_puts(s, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: asrun_sb_puts(s, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.2233720368547758e18) {
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
                asrun_sb_puts(s, buf);
            } else {
                asrun_float_repr(v->num_val, buf, sizeof(buf));
                asrun_sb_puts(s, buf);
            }
            break;
        case JSON_STRING: asrun_sb_json_string(s, v->str_val); break;
        case JSON_ARRAY:
            asrun_sb_putc(s, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) asrun_sb_puts(s, ", ");
                asrun_sb_dump(s, v->c.items[i]);
            }
            asrun_sb_putc(s, ']');
            break;
        case JSON_OBJECT:
            asrun_sb_putc(s, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) asrun_sb_puts(s, ", ");
                asrun_sb_json_string(s, v->c.keys[i]);
                asrun_sb_puts(s, ": ");
                asrun_sb_dump(s, v->c.items[i]);
            }
            asrun_sb_putc(s, '}');
            break;
        default: asrun_sb_puts(s, "null");
    }
}

static char *asrun_json_dumps_default(const json_t *v) {
    asrun_sb s;
    asrun_sb_init(&s);
    asrun_sb_dump(&s, v);
    return s.b;
}

/* ── Small shared helpers ───────────────────────────────────────────────── */

static bool asrun_is_terminal(const char *status) {
    return status && (strcmp(status, "completed") == 0 || strcmp(status, "failed") == 0 ||
                      strcmp(status, "cancelled") == 0 || strcmp(status, "interrupted") == 0);
}

static bool asrun_endswith(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

/* Object merge in source key order (dict.update semantics). dst/src objects. */
static void asrun_obj_update(json_t *dst, const json_t *src) {
    if (!dst || !src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
}

/* Set tables are string arrays. */
static bool asrun_set_has(const json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && strcmp(it->str_val, id) == 0) return true;
    }
    return false;
}

static void asrun_set_add(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || asrun_set_has(arr, id)) return;
    json_append(arr, json_string(id));
}

static void asrun_set_discard(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && strcmp(it->str_val, id) == 0) {
            json_free(arr->c.items[i]);
            for (size_t j = i; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            return;
        }
    }
}

static bool asrun_is_set_table(const char *name) {
    return strcmp(name, "run_idempotency_ids") == 0 ||
           strcmp(name, "stopping_run_ids") == 0 ||
           strcmp(name, "shutdown_interrupted_run_ids") == 0;
}

static json_t *asrun_state_table(json_t *state, const char *key) {
    json_t *t = json_obj_get(state, key);
    return (t && t->type == JSON_OBJECT) ? t : NULL;
}

/* _json_error: web.json_response(_openai_error(message, **kwargs), status=status). */
static json_t *asrun_json_error(const asrun_seams_t *s, const char *message, int status,
                                const json_t *kwargs) {
    json_t *empty = NULL;
    if (!kwargs) { empty = json_object(); kwargs = empty; }
    json_t *body = s->openai_error(s->ctx, message, kwargs);
    json_free(empty);
    json_t *desc = s->json_response(s->ctx, body, status, NULL);
    json_free(body);
    return desc;
}

/* malloc'd "<ExcType>: <msg>" — the oracle's catch-all error convention. */
static char *asrun_exc_str(const char *type, const char *msg) {
    size_t n = strlen(type) + strlen(msg) + 4;
    char *r = malloc(n);
    snprintf(r, n, "%s: %s", type, msg);
    return r;
}

/* ── State helpers ──────────────────────────────────────────────────────── */

static const char *asrun_state_dict_keys[] = {
    "run_statuses", "run_streams", "run_streams_created", "run_owners",
    "run_approval_sessions", "active_run_agents", "active_run_tasks",
};
static const char *asrun_state_set_keys[] = {
    "run_idempotency_ids", "stopping_run_ids", "shutdown_interrupted_run_ids",
};

json_t *asrun_state_new(const json_t *in_state) {
    json_t *state = json_object();
    for (size_t i = 0; i < sizeof(asrun_state_dict_keys) / sizeof(char *); i++) {
        const char *k = asrun_state_dict_keys[i];
        const json_t *src = in_state ? json_obj_get(in_state, k) : NULL;
        json_set(state, k, (src && src->type == JSON_OBJECT) ? json_copy(src) : json_object());
    }
    for (size_t i = 0; i < sizeof(asrun_state_set_keys) / sizeof(char *); i++) {
        const char *k = asrun_state_set_keys[i];
        const json_t *src = in_state ? json_obj_get(in_state, k) : NULL;
        json_set(state, k, (src && src->type == JSON_ARRAY) ? json_copy(src) : json_array());
    }
    const json_t *sra = in_state ? json_obj_get(in_state, "run_shutdown_requested_at") : NULL;
    json_set(state, "run_shutdown_requested_at",
             (sra && sra->type == JSON_NUMBER) ? json_copy(sra) : json_null());
    return state;
}

static int asrun_str_node_cmp(const void *a, const void *b) {
    const json_t *x = *(const json_t *const *)a;
    const json_t *y = *(const json_t *const *)b;
    return strcmp(x->str_val ? x->str_val : "", y->str_val ? y->str_val : "");
}

json_t *asrun_state_out(const json_t *state) {
    json_t *out = json_object();
    for (size_t i = 0; i < sizeof(asrun_state_dict_keys) / sizeof(char *); i++) {
        const char *k = asrun_state_dict_keys[i];
        const json_t *t = json_obj_get(state, k);
        json_set(out, k, (t && t->type == JSON_OBJECT) ? json_copy(t) : json_object());
    }
    for (size_t i = 0; i < sizeof(asrun_state_set_keys) / sizeof(char *); i++) {
        const char *k = asrun_state_set_keys[i];
        const json_t *t = json_obj_get(state, k);
        json_t *arr = (t && t->type == JSON_ARRAY) ? json_copy(t) : json_array();
        if (arr->c.count > 1) qsort(arr->c.items, arr->c.count, sizeof(json_t *), asrun_str_node_cmp);
        json_set(out, k, arr);
    }
    const json_t *sra = json_obj_get(state, "run_shutdown_requested_at");
    json_set(out, "run_shutdown_requested_at",
             (sra && sra->type == JSON_NUMBER) ? json_copy(sra) : json_null());
    return out;
}

json_t *asrun_req_state_new(const json_t *in) {
    json_t *req = json_object();
    const json_t *rs = in ? json_obj_get(in, "request_state") : NULL;
    const json_t *items = rs ? json_obj_get(rs, "items") : NULL;
    json_set(req, "items", (items && items->type == JSON_OBJECT) ? json_copy(items) : json_object());
    json_set(req, "attr", json_null());
    return req;
}

/* ── terminal_run_status ────────────────────────────────────────────────── */

/* PoP: asrun_terminal_run_status @ gateway/platforms/api_server_runs.py:terminal_run_status */
json_t *asrun_terminal_run_status(const json_t *result, char **status_out) {
    *status_out = NULL;
    const json_t *interrupted_v = result ? json_obj_get(result, "interrupted") : NULL;
    const json_t *failed_v = result ? json_obj_get(result, "failed") : NULL;
    const json_t *partial_v = result ? json_obj_get(result, "partial") : NULL;
    const json_t *completed_v = result ? json_obj_get(result, "completed") : NULL;
    bool interrupted = asrun_truthy(interrupted_v);
    /* `result.get("completed") is not False` — identity, not truthiness. */
    bool completed_is_false = completed_v && completed_v->type == JSON_BOOL && !completed_v->bool_val;
    bool finished = !interrupted && !asrun_truthy(failed_v) && !asrun_truthy(partial_v) &&
                    !completed_is_false;
    const char *status = interrupted ? "cancelled" : finished ? "completed" : "failed";
    json_t *fields = json_object();
    json_set(fields, "completed", json_bool(finished));
    json_set(fields, "partial", json_bool(asrun_truthy(partial_v)));
    json_set(fields, "interrupted", json_bool(interrupted));
    const json_t *ter = result ? json_obj_get(result, "turn_exit_reason") : NULL;
    if (!finished && asrun_truthy(ter)) {
        char *s = asrun_py_str(ter);
        json_set(fields, "turn_exit_reason", json_string(s));
        free(s);
    }
    const json_t *ps = result ? json_obj_get(result, "pending_steer") : NULL;
    if (asrun_truthy(ps)) json_set(fields, "pending_steer", json_copy(ps));
    *status_out = strdup(status);
    return fields;
}

/* ── _run_user_message ──────────────────────────────────────────────────── */

/* PoP: asrun_run_user_message @ gateway/platforms/api_server_runs.py:_run_user_message */
json_t *asrun_run_user_message(const json_t *raw_input, const asrun_seams_t *s,
                               json_t **err_out, char **exc_out) {
    *err_out = NULL;
    *exc_out = NULL;
    if (!asrun_truthy(raw_input)) {
        *err_out = asrun_json_error(s, "Missing 'input' field", 400, NULL);
        return NULL;
    }
    json_t *msg;
    if (raw_input->type == JSON_STRING) {
        msg = json_copy(raw_input);
    } else if (raw_input->type == JSON_ARRAY) {
        const json_t *last = raw_input->c.items[raw_input->c.count - 1];
        if (last && last->type == JSON_OBJECT) {
            const json_t *content = json_obj_get(last, "content");
            msg = content ? json_copy(content) : json_string("");
        } else {
            /* Live `raw_input[-1].get("content", "")` raises AttributeError on
             * a non-dict last element (the truthy gate already ruled out []). */
            char m[128];
            snprintf(m, sizeof(m), "'%s' object has no attribute 'get'",
                     asrun_py_type_name(last));
            *exc_out = asrun_exc_str("AttributeError", m);
            return NULL;
        }
    } else {
        msg = json_string("");
    }
    if (!asrun_truthy(msg)) {
        json_free(msg);
        *err_out = asrun_json_error(s, "No user message found in input", 400, NULL);
        return NULL;
    }
    return msg;
}

/* ── _tool_completed_preview ────────────────────────────────────────────── */

/* Code-point length of a UTF-8 string (Python len()). */
static size_t asrun_utf8_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Byte offset just past the first `cp` code points. */
static size_t asrun_utf8_cut(const char *s, size_t cp) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && n < cp) {
        if ((*p & 0xC0) != 0x80) n++;
        p++;
    }
    /* The loop exits on the leading byte count; walk past any continuation
     * bytes of the last counted code point so the cut never splits a char. */
    while ((*p & 0xC0) == 0x80) p++;
    return (size_t)(p - (const unsigned char *)s);
}

/* PoP: asrun_tool_completed_preview @ gateway/platforms/api_server_runs.py:_tool_completed_preview */
char *asrun_tool_completed_preview(const json_t *result, const asrun_seams_t *s) {
    if (!result || result->type == JSON_NULL) return strdup("");
    char *text;
    if (result->type == JSON_STRING) text = strdup(result->str_val ? result->str_val : "");
    else text = asrun_json_dumps_default(result);
    char *preview = s->redact(s->ctx, text);
    free(text);
    if (!preview) return strdup("");
    size_t limit = 500;   /* _TOOL_COMPLETED_PREVIEW_MAX_CHARS */
    if (asrun_utf8_len(preview) <= limit) return preview;
    size_t cut = asrun_utf8_cut(preview, limit - 3);
    char *r = malloc(cut + 4);
    memcpy(r, preview, cut);
    memcpy(r + cut, "...", 4);
    free(preview);
    return r;
}

/* ── _run_event ─────────────────────────────────────────────────────────── */

/* PoP: asrun_run_event @ gateway/platforms/api_server_runs.py:_run_event */
json_t *asrun_run_event(const char *run_id, const char *name, const json_t *fields,
                        const asrun_seams_t *s, char **exc_out) {
    *exc_out = NULL;
    /* Live `_run_event(run_id, name, **fields)` binds arguments BEFORE the body
     * runs: a fields key colliding with a positional parameter raises TypeError
     * (first colliding key in fields insertion order) and time.time() is never
     * reached, so the collision check precedes the clock seam. */
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count; i++) {
            const char *k = fields->c.keys[i];
            if (strcmp(k, "run_id") == 0 || strcmp(k, "name") == 0) {
                char m[128];
                snprintf(m, sizeof(m), "_run_event() got multiple values for argument '%s'", k);
                *exc_out = asrun_exc_str("TypeError", m);
                return NULL;
            }
        }
    }
    json_t *e = json_object();
    json_set(e, "event", json_string(name ? name : ""));
    json_set(e, "run_id", json_string(run_id ? run_id : ""));
    json_set(e, "timestamp", json_number(s->clock(s->ctx)));
    asrun_obj_update(e, fields);
    return e;
}

/* ── _call_id ───────────────────────────────────────────────────────────── */

/* PoP: asrun_call_id @ gateway/platforms/api_server_runs.py:_call_id */
json_t *asrun_call_id(const json_t *kw) {
    /* {"tool_call_id": kw["tool_call_id"]} if kw.get("tool_call_id") else {} —
     * a truthy .get guarantees the key is present, so no KeyError is reachable. */
    json_t *out = json_object();
    const json_t *v = kw ? json_obj_get(kw, "tool_call_id") : NULL;
    if (asrun_truthy(v)) json_set(out, "tool_call_id", json_copy(v));
    return out;
}

/* ── _room_permission_for ───────────────────────────────────────────────── */

/* PoP: asrun_room_permission_for @ gateway/platforms/api_server_runs.py:_room_permission_for */
const char *asrun_room_permission_for(const json_t *request) {
    const char *path = json_get_str(request, "path", "");
    const char *method = json_get_str(request, "method", "");
    /* _ROOM_CONTROL_PERMISSIONS in insertion order. */
    static const char *suffixes[] = { "/stop", "/resolve-unknown", "/approval", "/clarify" };
    static const char *perms[] = { "stop", "stop", "approve", "approve" };
    for (size_t i = 0; i < 4; i++)
        if (asrun_endswith(path, suffixes[i])) return perms[i];
    return strcmp(method, "GET") == 0 ? "status" : "dispatch";
}

/* ── room_run_scope ─────────────────────────────────────────────────────── */

/* PoP: asrun_room_scope @ gateway/platforms/api_server_runs.py:room_run_scope */
char *asrun_room_scope(const json_t *claims, char **err_out) {
    *err_out = NULL;
    static const char *keys[] = {
        "room_id", "home_install_id", "authority_gateway_id", "authority_epoch",
        "member_id", "target_install_id", "target_profile",
    };
    asrun_sb joined;
    asrun_sb_init(&joined);
    for (size_t i = 0; i < 7; i++) {
        const json_t *v = claims ? json_obj_get(claims, keys[i]) : NULL;
        if (!v) {
            size_t n = strlen(keys[i]) + 16;
            char *e = malloc(n);
            snprintf(e, n, "KeyError: '%s'", keys[i]);
            *err_out = e;
            free(joined.b);
            return NULL;
        }
        char *part = asrun_py_str(v);
        asrun_sb_puts(&joined, part);
        free(part);
        if (i < 6) asrun_sb_putc(&joined, '\0');
    }
    char *hex = malloc(65);
    asrun_sha256_hex((const unsigned char *)joined.b, joined.len, hex);
    free(joined.b);
    return hex;
}

/* ── _remember_room_retention ───────────────────────────────────────────── */

/* PoP: asrun_remember_room_retention @ gateway/platforms/api_server_runs.py:_remember_room_retention */
void asrun_remember_room_retention(json_t *req_state, const json_t *claims,
                                   const asrun_seams_t *s, char **err_out) {
    *err_out = NULL;
    /* float(claims.get("status_expires_at") or claims.get("expires_at") or 0) */
    const json_t *a = claims ? json_obj_get(claims, "status_expires_at") : NULL;
    const json_t *b = claims ? json_obj_get(claims, "expires_at") : NULL;
    const json_t *pick = asrun_truthy(a) ? a : asrun_truthy(b) ? b : NULL;
    double value = 0.0;
    if (pick) {
        bool ok = false;
        value = asrun_py_float(pick, &ok);
        if (!ok) {
            *err_out = strdup("ValueError: could not convert string to float");
            return;
        }
    }
    const char *exc_type = NULL;
    bool stored = s->request_set(s->ctx, req_state, "hermes.room_run_retention_until",
                                 value, &exc_type);
    if (!stored) {
        if (exc_type && (strcmp(exc_type, "AttributeError") == 0 ||
                         strcmp(exc_type, "TypeError") == 0)) {
            json_set(req_state, "attr", json_number(value));
        } else {
            *err_out = asrun_exc_str(exc_type ? exc_type : "Exception",
                                     exc_type ? exc_type : "");
        }
    }
}

/* ── _run_idempotency_scope ─────────────────────────────────────────────── */

/* PoP: asrun_run_idempotency_scope @ gateway/platforms/api_server_runs.py:_run_idempotency_scope */
char *asrun_run_idempotency_scope(const json_t *request, json_t *req_state,
                                  const asrun_seams_t *s, char **err_out) {
    *err_out = NULL;
    const char *token = s->room_grant_token(s->ctx, request);
    if (token && token[0] != '\0') {
        const char *permission = asrun_room_permission_for(request);
        bool raised = false;
        const char *etype = NULL, *emsg = NULL;
        json_t *claims = s->room_run_claims(s->ctx, request, permission, &raised, &etype, &emsg);
        if (raised) {
            *err_out = asrun_exc_str(etype ? etype : "Exception", emsg ? emsg : "");
            return NULL;
        }
        asrun_remember_room_retention(req_state, claims, s, err_out);
        if (*err_out) { json_free(claims); return NULL; }
        char *hex = asrun_room_scope(claims, err_out);
        json_free(claims);
        return hex;
    }
    const char *profile = s->request_profile(s->ctx);
    const char *p = (profile && profile[0] != '\0') ? profile : "default";
    const char *key = s->expected_api_key(s->ctx);
    const char *k = (key && key[0] != '\0') ? key : "unauthenticated-test-listener";
    asrun_sb joined;
    asrun_sb_init(&joined);
    asrun_sb_puts(&joined, p);
    asrun_sb_putc(&joined, '\0');
    asrun_sb_puts(&joined, k);
    char *hex = malloc(65);
    asrun_sha256_hex((const unsigned char *)joined.b, joined.len, hex);
    free(joined.b);
    return hex;
}

/* ── _check_run_auth ────────────────────────────────────────────────────── */

/* PoP: asrun_check_run_auth @ gateway/platforms/api_server_runs.py:_check_run_auth */
json_t *asrun_check_run_auth(const json_t *request, const char *permission, const asrun_seams_t *s) {
    const char *token = s->room_grant_token(s->ctx, request);
    if (!token || token[0] == '\0') return s->check_auth(s->ctx, request);
    bool raised = false;
    const char *etype = NULL, *emsg = NULL;
    json_t *claims = s->room_run_claims(s->ctx, request, permission, &raised, &etype, &emsg);
    if (raised) {
        /* Live _room_grant_error_response: revoked/superseded grants need
         * reauthorization (403), anything else is invalid/expired (401). */
        bool reauth = etype && strcmp(etype, "RoomGrantReauthorizationRequired") == 0;
        const char *message = reauth ? "Room authorization needs to be renewed."
                                     : "Room authorization is invalid or expired.";
        const char *code = reauth ? "room_reauthorization_required" : "invalid_room_grant";
        json_t *kwargs = json_object();
        json_set(kwargs, "err_type", json_string("gateway_auth_error"));
        json_set(kwargs, "code", json_string(code));
        json_t *desc = asrun_json_error(s, message, reauth ? 403 : 401, kwargs);
        json_free(kwargs);
        return desc;
    }
    json_free(claims);
    return NULL;
}

/* ── _owner_alive ───────────────────────────────────────────────────────── */

/* PoP: asrun_owner_alive @ gateway/platforms/api_server_runs.py:_owner_alive */
bool asrun_owner_alive(long owner_pid, long owner_started, const asrun_seams_t *s) {
    if (owner_pid <= 0) return false;
    bool raised = false;
    bool exists = s->pid_exists(s->ctx, owner_pid, &raised);
    if (raised || !exists) return false;
    if (!owner_started) return true;
    bool is_none = false;
    double current = s->process_start_time(s->ctx, owner_pid, &raised, &is_none);
    if (raised) return false;
    if (is_none || current == 0.0) current = 0.0;   /* `or 0` */
    bool match = s->fingerprints_match(s->ctx, owner_started, current, &raised);
    if (raised) return false;
    return match;
}

/* ── _set_run_status ────────────────────────────────────────────────────── */

/* PoP: asrun_set_run_status @ gateway/platforms/api_server_runs.py:_set_run_status */
json_t *asrun_set_run_status(json_t *state, const char *run_id, const char *status,
                             const json_t *fields, const asrun_seams_t *s) {
    json_t *statuses = asrun_state_table(state, "run_statuses");
    double now = s->clock(s->ctx);
    json_t *current = json_obj_get(statuses, run_id);
    bool is_new = !current;
    if (is_new) current = json_object();
    /* previous_status = str(current.get("status") or "") */
    const json_t *prev_v = json_obj_get(current, "status");
    char *previous_status = asrun_truthy(prev_v) ? asrun_py_str(prev_v) : strdup("");
    /* current.update({"object", "run_id", "status", "updated_at"}) */
    json_set(current, "object", json_string("hermes.run"));
    json_set(current, "run_id", json_string(run_id));
    json_set(current, "status", json_string(status));
    json_set(current, "updated_at", json_number(now));
    /* current.setdefault("created_at", fields.pop("created_at", now)) — the
     * pop is eager even when created_at is already present. */
    json_t *fcopy = fields ? json_copy(fields) : json_object();
    const json_t *created_v = json_obj_get(fcopy, "created_at");
    json_t *created_val = created_v ? json_copy(created_v) : json_number(now);
    if (created_v) json_obj_del(fcopy, "created_at");
    if (!json_has(current, "created_at")) json_set(current, "created_at", created_val);
    else json_free(created_val);
    asrun_obj_update(current, fcopy);
    /* shutdown stamp rides every non-terminal status once shutdown began */
    const json_t *sra = json_obj_get(state, "run_shutdown_requested_at");
    if (sra && sra->type != JSON_NULL && !asrun_is_terminal(status)) {
        if (!json_has(current, "shutdown_requested_at"))
            json_set(current, "shutdown_requested_at", json_copy(sra));
    }
    if (strcmp(status, "waiting_for_approval") != 0) json_obj_del(current, "approval");
    if (is_new) json_set(statuses, run_id, current);   /* adopts current */
    /* should_persist: status change, terminal, or a special field name.
     * field_names was captured BEFORE the created_at pop. */
    bool special = false;
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count && !special; i++) {
            const char *k = fields->c.keys[i];
            special = strcmp(k, "output") == 0 || strcmp(k, "error") == 0 ||
                      strcmp(k, "usage") == 0 || strcmp(k, "pending_steer") == 0 ||
                      strcmp(k, "session_id") == 0 || strcmp(k, "shutdown_requested_at") == 0;
        }
    }
    bool should_persist = strcmp(status, previous_status) != 0 || asrun_is_terminal(status) || special;
    free(previous_status);
    json_t *ids = json_obj_get(state, "run_idempotency_ids");
    if (asrun_set_has(ids, run_id) && should_persist) {
        bool raised = false;
        const char *etype = NULL, *emsg = NULL;
        s->store_update_status(s->ctx, run_id, current, &raised, &etype, &emsg);
        if (raised) {
            char msg[512];
            snprintf(msg, sizeof(msg), "[api_server] failed to persist idempotent run status %s", run_id);
            s->log(s->ctx, "exception", msg);
        }
    }
    json_free(fcopy);
    return json_copy(current);
}

/* ── _mark_shutdown_requested ───────────────────────────────────────────── */

/* PoP: asrun_mark_shutdown_requested @ gateway/platforms/api_server_runs.py:_mark_shutdown_requested */
long asrun_mark_shutdown_requested(json_t *state, const asrun_seams_t *s) {
    json_t *sra = json_obj_get(state, "run_shutdown_requested_at");
    if (!sra || sra->type == JSON_NULL) {
        json_set(state, "run_shutdown_requested_at", json_number(s->clock(s->ctx)));
        sra = json_obj_get(state, "run_shutdown_requested_at");
    }
    json_t *statuses = asrun_state_table(state, "run_statuses");
    /* list(self._run_statuses.items()) — snapshot before the writes. */
    size_t n = statuses->c.count;
    char **ids = malloc(n * sizeof(char *));
    for (size_t i = 0; i < n; i++) ids[i] = strdup(statuses->c.keys[i]);
    long marked = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *current = json_obj_get(statuses, ids[i]);
        const json_t *st = current ? json_obj_get(current, "status") : NULL;
        bool terminal = st && st->type == JSON_STRING && asrun_is_terminal(st->str_val);
        if (terminal || (current && json_has(current, "shutdown_requested_at"))) {
            free(ids[i]);
            continue;
        }
        char *status_str = asrun_truthy(st) ? asrun_py_str(st) : strdup("running");
        json_t *fields = json_object();
        json_set(fields, "shutdown_requested_at", json_copy(sra));
        json_t *updated = asrun_set_run_status(state, ids[i], status_str, fields, s);
        json_free(updated);
        json_free(fields);
        free(status_str);
        free(ids[i]);
        marked++;
    }
    free(ids);
    return marked;
}

/* ── _forget_run / _release_run_owner_if_forgotten ──────────────────────── */

/* PoP: asrun_release_run_owner_if_forgotten @ gateway/platforms/api_server_runs.py:_release_run_owner_if_forgotten */
void asrun_release_run_owner_if_forgotten(json_t *state, const char *run_id) {
    static const char *live[] = {
        "run_statuses", "active_run_agents", "active_run_tasks",
        "run_streams", "run_approval_sessions",
    };
    for (size_t i = 0; i < 5; i++) {
        json_t *t = json_obj_get(state, live[i]);
        if (t && t->type == JSON_OBJECT && json_has(t, run_id)) return;
    }
    json_t *owners = asrun_state_table(state, "run_owners");
    if (owners) json_obj_del(owners, run_id);
}

/* PoP: asrun_forget_run @ gateway/platforms/api_server_runs.py:_forget_run */
void asrun_forget_run(json_t *state, const char *run_id, const json_t *tables) {
    if (tables && tables->type == JSON_ARRAY) {
        for (size_t i = 0; i < tables->c.count; i++) {
            const json_t *name = tables->c.items[i];
            if (!name || name->type != JSON_STRING) continue;
            json_t *table = json_obj_get(state, name->str_val);
            if (!table) continue;
            if (asrun_is_set_table(name->str_val)) asrun_set_discard(table, run_id);
            else if (table->type == JSON_OBJECT) json_obj_del(table, run_id);
        }
    }
    asrun_release_run_owner_if_forgotten(state, run_id);
}

/* ── _durable_run_status ────────────────────────────────────────────────── */

/* PoP: asrun_durable_run_status @ gateway/platforms/api_server_runs.py:_durable_run_status */
json_t *asrun_durable_run_status(json_t *state, const json_t *request, json_t *req_state,
                                 const char *run_id, const asrun_seams_t *s, char **err_out) {
    *err_out = NULL;
    json_t *statuses = asrun_state_table(state, "run_statuses");
    json_t *canonical = s->run_projection(s->ctx, run_id);
    if (canonical) {
        /* {"object": "hermes.run", **retained-subset, **canonical} */
        json_t *out = json_object();
        json_set(out, "object", json_string("hermes.run"));
        const json_t *retained = json_obj_get(statuses, run_id);
        if (!asrun_truthy(retained)) retained = NULL;
        static const char *keep[] = { "model", "runtime", "created_at" };
        for (size_t i = 0; i < 3; i++) {
            if (retained && json_has(retained, keep[i]))
                json_set(out, keep[i], json_copy(json_obj_get(retained, keep[i])));
        }
        asrun_obj_update(out, canonical);
        json_free(canonical);
        return out;
    }
    json_t *status = json_obj_get(statuses, run_id);
    json_t *ids = json_obj_get(state, "run_idempotency_ids");
    /* Live `status = self._run_statuses.get(run_id); if status is not None` —
     * .get returns None for BOTH an absent key and a present-null value, so a
     * JSON null status falls through to durable hydration, not the return. */
    if (status && status->type != JSON_NULL) {
        if (asrun_set_has(ids, run_id)) {
            char *scope = asrun_run_idempotency_scope(request, req_state, s, err_out);
            if (*err_out) return NULL;
            double retention = s->room_retention_until(s->ctx, request);
            s->store_extend_retention(s->ctx, scope, run_id, retention);
            free(scope);
        }
        return json_copy(status);
    }
    char *scope = asrun_run_idempotency_scope(request, req_state, s, err_out);
    if (*err_out) return NULL;
    double retention = s->room_retention_until(s->ctx, request);
    json_t *record = s->store_status_for_run(s->ctx, scope, run_id, retention);
    if (!record) { free(scope); return NULL; }
    const json_t *rec_status = record->type == JSON_OBJECT ? json_obj_get(record, "status") : NULL;
    if (!rec_status) {
        *err_out = strdup("KeyError: 'status'");
        json_free(record);
        free(scope);
        return NULL;
    }
    json_t *hydrated = json_copy(rec_status);
    const json_t *st = hydrated->type == JSON_OBJECT ? json_obj_get(hydrated, "status") : NULL;
    bool terminal = st && st->type == JSON_STRING && asrun_is_terminal(st->str_val);
    if (!terminal) {
        /* Live reads owner_pid/owner_started from the RECORD (record.get(...)),
         * NOT from record["status"]; keep record alive until after this branch. */
        const json_t *pid_v = json_obj_get(record, "owner_pid");
        const json_t *started_v = json_obj_get(record, "owner_started");
        long pid = asrun_truthy(pid_v) ? asrun_py_int(pid_v) : 0;
        long started = asrun_truthy(started_v) ? asrun_py_int(started_v) : 0;
        if (!asrun_owner_alive(pid, started, s)) {
            json_set(hydrated, "status", json_string("interrupted"));
            json_set(hydrated, "error", json_string("The gateway restarted before this run settled."));
            json_set(hydrated, "last_event", json_string("run.interrupted"));
            json_set(hydrated, "updated_at", json_number(s->clock(s->ctx)));
            bool raised = false;
            const char *etype = NULL, *emsg = NULL;
            s->store_update_status(s->ctx, run_id, hydrated, &raised, &etype, &emsg);
            if (raised) {
                *err_out = asrun_exc_str(etype ? etype : "Exception", emsg ? emsg : "");
                json_free(hydrated);
                json_free(record);
                free(scope);
                return NULL;
            }
        }
    }
    json_free(record);
    json_set(statuses, run_id, hydrated);   /* adopts hydrated (new key) */
    asrun_set_add(ids, run_id);
    json_t *owners = asrun_state_table(state, "run_owners");
    json_set(owners, run_id, json_string(scope));
    free(scope);
    return json_copy(hydrated);
}

/* ── _accepted_response ─────────────────────────────────────────────────── */

/* PoP: asrun_accepted_response @ gateway/platforms/api_server_runs.py:_accepted_response */
json_t *asrun_accepted_response(const char *run_id, const json_t *status,
                                const char *gateway_session_key, bool replayed,
                                const asrun_seams_t *s) {
    json_t *headers = json_object();
    if (replayed) json_set(headers, "Idempotency-Replayed", json_string("true"));
    if (gateway_session_key && gateway_session_key[0] != '\0')
        json_set(headers, "X-Hermes-Session-Key", json_string(gateway_session_key));
    json_t *body = json_object();
    json_set(body, "run_id", json_string(run_id ? run_id : ""));
    json_set(body, "status", status ? json_copy(status) : json_null());
    json_set(body, "replayed", json_bool(replayed));
    json_t *desc = s->json_response(s->ctx, body, 202, headers);
    json_free(body);
    json_free(headers);
    return desc;
}

/* ── _replay_or_conflict ────────────────────────────────────────────────── */

/* PoP: asrun_replay_or_conflict @ gateway/platforms/api_server_runs.py:_replay_or_conflict */
json_t *asrun_replay_or_conflict(json_t *state, const json_t *request, json_t *req_state,
                                 const char *outcome, const json_t *record,
                                 const char *gateway_session_key, const asrun_seams_t *s,
                                 char **err_out) {
    *err_out = NULL;
    if (outcome && strcmp(outcome, "conflict") == 0) {
        json_t *kwargs = json_object();
        json_set(kwargs, "code", json_string("idempotency_key_conflict"));
        json_t *desc = asrun_json_error(
            s, "Idempotency-Key was already used with a different request payload", 409, kwargs);
        json_free(kwargs);
        return desc;
    }
    const json_t *rid_v = record ? json_obj_get(record, "run_id") : NULL;
    if (!rid_v) { *err_out = strdup("KeyError: 'run_id'"); return NULL; }
    char *original_id = asrun_py_str(rid_v);
    json_t *dur = asrun_durable_run_status(state, request, req_state, original_id, s, err_out);
    if (*err_out) { free(original_id); return NULL; }
    /* `self._durable_run_status(...) or record["status"]` — an EMPTY dict is
     * falsy and falls back to the stored record. */
    const json_t *status_obj = asrun_truthy(dur) ? dur : json_obj_get(record, "status");
    if (!status_obj) {
        *err_out = strdup("KeyError: 'status'");
        json_free(dur);
        free(original_id);
        return NULL;
    }
    const json_t *st = json_obj_get(status_obj, "status");
    json_t *status_node = st ? json_copy(st) : json_string("queued");
    json_t *desc = asrun_accepted_response(original_id, status_node, gateway_session_key, true, s);
    json_free(status_node);
    json_free(dur);
    free(original_id);
    return desc;
}

/* ── _resolve_conversation_history ──────────────────────────────────────── */

/* PoP: asrun_resolve_conversation_history @ gateway/platforms/api_server_runs.py:_resolve_conversation_history */
json_t *asrun_resolve_conversation_history(const json_t *body, const json_t *raw_input,
                                           const asrun_seams_t *s, json_t **instructions_out,
                                           json_t **stored_out, json_t **err_out) {
    *instructions_out = NULL;
    *stored_out = NULL;
    *err_out = NULL;
    const json_t *instructions_v = body ? json_obj_get(body, "instructions") : NULL;
    /* JSON null is Python None: the chain fallback below adopts stored
     * instructions only when instructions IS None. */
    json_t *instructions = (instructions_v && instructions_v->type != JSON_NULL)
                               ? json_copy(instructions_v) : NULL;
    const json_t *prev = body ? json_obj_get(body, "previous_response_id") : NULL;
    json_t *history = json_array();
    const json_t *raw_history = body ? json_obj_get(body, "conversation_history") : NULL;
    if (asrun_truthy(raw_history)) {
        if (raw_history->type != JSON_ARRAY) {
            json_free(history);
            *instructions_out = instructions;
            *err_out = asrun_json_error(
                s, "'conversation_history' must be an array of message objects", 400, NULL);
            return json_array();
        }
        for (size_t i = 0; i < raw_history->c.count; i++) {
            const json_t *entry = raw_history->c.items[i];
            if (!entry || entry->type != JSON_OBJECT ||
                !json_has(entry, "role") || !json_has(entry, "content")) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "conversation_history[%zu] must have 'role' and 'content' fields", i);
                json_free(history);
                *instructions_out = instructions;
                *err_out = asrun_json_error(s, msg, 400, NULL);
                return json_array();
            }
            json_t *m = json_object();
            char *role = asrun_py_str(json_obj_get(entry, "role"));
            char *content = asrun_py_str(json_obj_get(entry, "content"));
            json_set(m, "role", json_string(role));
            json_set(m, "content", json_string(content));
            free(role);
            free(content);
            json_append(history, m);
        }
        if (asrun_truthy(prev))
            s->log(s->ctx, "debug",
                   "Both conversation_history and previous_response_id provided; using conversation_history");
    }
    if (json_len(history) == 0 && asrun_truthy(prev)) {
        const char *prev_id = (prev->type == JSON_STRING && prev->str_val) ? prev->str_val : "";
        json_t *stored = s->response_store_get(s->ctx, prev_id);
        if (asrun_truthy(stored)) {
            const json_t *hist = json_obj_get(stored, "conversation_history");
            if (hist && hist->type == JSON_ARRAY)
                for (size_t i = 0; i < hist->c.count; i++)
                    json_append(history, json_copy(hist->c.items[i]));
            const json_t *sid = json_obj_get(stored, "session_id");
            *stored_out = sid ? json_copy(sid) : NULL;
            if (!instructions) {
                const json_t *si = json_obj_get(stored, "instructions");
                instructions = si ? json_copy(si) : NULL;
            }
        }
        json_free(stored);
    }
    if (json_len(history) == 0 && raw_input && raw_input->type == JSON_ARRAY &&
        raw_input->c.count > 1) {
        for (size_t i = 0; i + 1 < raw_input->c.count; i++) {
            const json_t *msg = raw_input->c.items[i];
            if (!msg || msg->type != JSON_OBJECT) continue;
            const json_t *role = json_obj_get(msg, "role");
            const json_t *content = json_obj_get(msg, "content");
            if (!asrun_truthy(role) || !asrun_truthy(content)) continue;
            char *content_str;
            if (content->type == JSON_ARRAY) {
                /* flatten multi-part text blocks: " ".join(p.get("text", "")
                 * for p in content if isinstance(p, dict) and p.get("type") == "text") */
                asrun_sb joined;
                asrun_sb_init(&joined);
                bool first = true;
                for (size_t j = 0; j < content->c.count; j++) {
                    const json_t *p = content->c.items[j];
                    if (!p || p->type != JSON_OBJECT) continue;
                    const json_t *ty = json_obj_get(p, "type");
                    if (!ty || ty->type != JSON_STRING || strcmp(ty->str_val, "text") != 0) continue;
                    if (!first) asrun_sb_putc(&joined, ' ');
                    first = false;
                    const json_t *text = json_obj_get(p, "text");
                    if (text && text->type == JSON_STRING && text->str_val)
                        asrun_sb_puts(&joined, text->str_val);
                }
                content_str = joined.b;
            } else {
                content_str = asrun_py_str(content);
            }
            json_t *m = json_object();
            json_set(m, "role", json_copy(role));
            json_set(m, "content", json_string(content_str));
            free(content_str);
            json_append(history, m);
        }
    }
    *instructions_out = instructions;
    return history;
}