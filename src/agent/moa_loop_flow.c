/*
 * moa_loop_flow.c — fan-out / aggregation FLOW cores + leaves of
 * agent/moa_loop.py (LIVE authority). See include/moa_loop_flow.h for the
 * universal-seam contract, the seam name table and the value/state model.
 *
 * Decision sequences only: every live model-call / sibling / clock effect
 * enters through mlf_io_t.call. Python truthiness, `or` short-circuits,
 * `is None` vs truthy tests, exception catch scopes (Exception only where the
 * live code catches Exception; a scripted BaseException propagates past those
 * guards exactly as live), comprehension call ORDER and the fan-out emission
 * order are transcribed per call site.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). SHA-256 is
 * inline so _hash_messages needs no libhash object. The harness #includes this
 * .c directly.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "moa_loop_flow.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── module constants (moa_loop.py) ── */
#define MLF_INTERRUPTED_NOTE "[skipped: interrupted by user]"
#define MLF_ADVISORY_INSTRUCTION \
    "[The conversation above is the current state of the task. Give your " \
    "most intelligent judgement: what is going on, what should happen next, " \
    "what risks or mistakes you see, and how the acting agent should " \
    "proceed.]"
#define MLF_STALE_GUIDANCE_NOTE \
    "This guidance was produced earlier in this turn, before the tool results " \
    "below it. Check the transcript before acting on it: a step it suggests " \
    "may already have run, and repeating a completed tool call is never the " \
    "next step.\n"

/* ── JSON / Python-value helpers ─────────────────────────────────────────── */

static bool mlf_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool mlf_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool mlf_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool mlf_is_str(const json_t *v) { return v && v->type == JSON_STRING; }

static const char *mlf_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python truthiness (NULL pointer == None). */
static bool mlf_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static double mlf_num(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

/* float(x) for the number|string fixture domain. */
static double mlf_to_double(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING && v->str_val) return strtod(v->str_val, NULL);
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

static void mlf_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str() of a scalar (None/bool/str/number). dict/list fall back to the
 * JSON serialization (outside the _hash_messages fixture domain). malloc'd. */
static char *mlf_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                mlf_float_repr(d, buf, sizeof(buf));
            return strdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}

/* str(x or "") — empty string when x is falsy (NOT "None"). malloc'd. */
static char *mlf_pystr_or_empty(const json_t *v) {
    if (!mlf_truth(v)) return strdup("");
    return mlf_pystr(v);
}

/* str.strip(): ASCII whitespace + U+0085 / U+00A0. malloc'd copy. */
static char *mlf_strip(const char *s) {
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) { p++; continue; }
        if (c == 0xC2 && ((unsigned char)p[1] == 0x85 || (unsigned char)p[1] == 0xA0)) { p += 2; continue; }
        break;
    }
    const char *end = p + strlen(p);
    while (end > p) {
        unsigned char c = (unsigned char)end[-1];
        if (c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) { end--; continue; }
        if (end - p >= 2 && (unsigned char)end[-2] == 0xC2
            && (c == 0x85 || c == 0xA0)) { end -= 2; continue; }
        break;
    }
    size_t n = (size_t)(end - p);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

static char *mlf_lower_dup(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

static bool mlf_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* Python-style modulo (result has the sign of the divisor). */
static long long mlf_py_mod(long long a, long long n) {
    if (n == 0) return 0;
    long long r = a % n;
    if (r < 0) r += (n > 0 ? n : -n);
    return r;
}

/* ── args builders (None-valued keys dropped on both sides) ── */

static void mlf_arg_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void mlf_arg_j(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void mlf_arg_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
static void mlf_arg_n(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}

/* Universal seam call: frees args, returns the NEW scripted result (NULL ==
 * None) or transfers a raise-spec into *exc. */
static json_t *mlf_call(const mlf_io_t *io, const char *name, json_t *args,
                        json_t **exc) {
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__")) {
        *exc = r;
        return NULL;
    }
    return r;
}

/* Swallow a caught exception (live `except Exception` / suppress(Exception)).
 * A BaseException-only class (CancelledError / SystemExit / KeyboardInterrupt /
 * GeneratorExit) is NOT caught by `except Exception` live, so it propagates:
 * returns false and leaves *exc set. Returns true when swallowed. */
static bool mlf_swallow(json_t **exc) {
    if (!*exc) return true;
    const char *cls = json_get_str(*exc, "__raise__", "");
    static const char *const base_only[] = {
        "CancelledError", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
    };
    for (size_t i = 0; i < sizeof(base_only) / sizeof(base_only[0]); i++)
        if (mlf_str_eq(cls, base_only[i])) return false;   /* propagate */
    json_free(*exc); *exc = NULL;
    return true;
}

/* ── CanonicalUsage / _RefAccounting value model ─────────────────────────── */

static const char *const MLF_USAGE_FIELDS[6] = {
    "input_tokens", "output_tokens", "cache_read_tokens",
    "cache_write_tokens", "reasoning_tokens", "request_count",
};

static bool mlf_is_usage(const json_t *v) {
    return mlf_is_obj(v) && mlf_str_eq(json_get_str(v, "_type", ""), "CanonicalUsage");
}
static bool mlf_is_acct(const json_t *v) {
    return mlf_is_obj(v) && mlf_str_eq(json_get_str(v, "_type", ""), "_RefAccounting");
}

/* CanonicalUsage() — request_count defaults to 1, the rest 0. */
static json_t *mlf_usage_fresh(void) {
    json_t *u = json_object();
    json_set(u, "_type", json_string("CanonicalUsage"));
    for (int i = 0; i < 6; i++)
        json_set(u, MLF_USAGE_FIELDS[i], json_number(i == 5 ? 1.0 : 0.0));
    return u;
}

/* CanonicalUsage.__add__: sum the six fields, drop per-response detail. */
static json_t *mlf_usage_add(const json_t *a, const json_t *b) {
    json_t *u = json_object();
    json_set(u, "_type", json_string("CanonicalUsage"));
    for (int i = 0; i < 6; i++) {
        double av = json_get_num(a, MLF_USAGE_FIELDS[i], 0.0);
        double bv = json_get_num(b, MLF_USAGE_FIELDS[i], 0.0);
        json_set(u, MLF_USAGE_FIELDS[i], json_number(av + bv));
    }
    return u;
}

/* _RefAccounting(usage=..., everything else default None). */
static json_t *mlf_acct_fresh(json_t *usage) {
    json_t *a = json_object();
    json_set(a, "_type", json_string("_RefAccounting"));
    json_set(a, "usage", usage ? usage : json_null());
    json_set(a, "cost_usd", json_null());
    json_set(a, "cost_status", json_null());
    json_set(a, "cost_source", json_null());
    json_set(a, "messages", json_null());
    json_set(a, "output", json_null());
    json_set(a, "model", json_null());
    json_set(a, "provider", json_null());
    json_set(a, "temperature", json_null());
    return a;
}

/* A reference-output 3-tuple [label, text, acct] element accessor. */
static const json_t *mlf_out_label(const json_t *o) { return json_get(o, 0); }
static const json_t *mlf_out_text(const json_t *o) { return json_get(o, 1); }
static const json_t *mlf_out_acct(const json_t *o) { return json_get(o, 2); }

/* ── SHA-256 (inline; _hash_messages digest, no libhash object) ── */

static uint32_t mlf_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void mlf_sha256_hex(const unsigned char *data, size_t len, char out[65]) {
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
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };
    uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    size_t total = len + 1 + 8;
    size_t padded = ((total + 63) / 64) * 64;
    unsigned char *msg = (unsigned char *)calloc(padded, 1);
    if (!msg) { out[0] = '\0'; return; }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bitlen = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        msg[padded - 1 - (size_t)i] = (unsigned char)(bitlen >> (8 * i));
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[off + (size_t)i * 4] << 24)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 1] << 16)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 2] << 8)
                 | (uint32_t)msg[off + (size_t)i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = mlf_rotr(w[i - 15], 7) ^ mlf_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = mlf_rotr(w[i - 2], 17) ^ mlf_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = mlf_rotr(e, 6) ^ mlf_rotr(e, 11) ^ mlf_rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = mlf_rotr(a, 2) ^ mlf_rotr(a, 13) ^ mlf_rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(msg);
    static const char hexd[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char)(h[i] >> (24 - 8 * j));
            out[i * 8 + j * 2] = hexd[byte >> 4];
            out[i * 8 + j * 2 + 1] = hexd[byte & 0x0F];
        }
    out[64] = '\0';
}

/* ── dynamic string buffer ── */
typedef struct { char *p; size_t n, cap; } mlf_buf_t;
static void mlf_buf_init(mlf_buf_t *b) { b->p = NULL; b->n = 0; b->cap = 0; }
static void mlf_buf_add(mlf_buf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}
static void mlf_buf_adds(mlf_buf_t *b, const char *s) { mlf_buf_add(b, s, strlen(s)); }

/* ── 1. _field ──────────────────────────────────────────────────────────────
 *   if obj is None: return None
 *   return obj.get(name) if isinstance(obj, dict) else getattr(obj, name, None)
 */
/* PoP: mlf_field @ agent/moa_loop.py:_field */
json_t *mlf_field(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *obj = json_obj_get(in, "obj");
    const char *name = json_get_str(in, "name", "");
    if (mlf_is_null(obj)) return json_null();
    const json_t *src = obj;
    if (mlf_is_obj(obj) && json_obj_get(obj, "__attr__"))
        src = json_obj_get(obj, "__attr__");   /* attribute double */
    if (!mlf_is_obj(src)) return json_null();  /* getattr(scalar, name, None) */
    const json_t *v = json_obj_get(src, name);
    return v ? json_copy(v) : json_null();
}

/* ── 2. _slot_labels ────────────────────────────────────────────────────────
 *   return ", ".join(_slot_label(slot) for slot in slots)
 */
/* PoP: mlf_slot_labels @ agent/moa_loop.py:_slot_labels */
json_t *mlf_slot_labels(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *slots = json_obj_get(in, "slots");
    mlf_buf_t b; mlf_buf_init(&b);
    if (mlf_is_arr(slots)) {
        for (size_t i = 0; i < slots->c.count; i++) {
            json_t *a = json_object();
            mlf_arg_j(a, "slot", slots->c.items[i]);
            json_t *lbl = mlf_call(io, "_slot_label", a, exc);
            if (*exc) { free(b.p); return NULL; }
            if (i) mlf_buf_adds(&b, ", ");
            char *s = mlf_pystr(lbl);
            mlf_buf_adds(&b, s ? s : "");
            free(s); json_free(lbl);
        }
    }
    json_t *ret = json_string(b.p ? b.p : "");
    free(b.p);
    return ret;
}

/* ── 3. _agent_cache_opts ───────────────────────────────────────────────────
 *   if agent is None: return None, None, False
 *   return (getattr(agent, "_cache_disabled", None), getattr(agent, "_cache_ttl", None),
 *           bool(getattr(agent, "_cache_ttl_auto", False)))
 */
/* PoP: mlf_agent_cache_opts @ agent/moa_loop.py:_agent_cache_opts */
json_t *mlf_agent_cache_opts(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *agent = json_obj_get(in, "agent");
    json_t *ret = json_array();
    if (mlf_is_null(agent)) {
        json_append(ret, json_null());
        json_append(ret, json_null());
        json_append(ret, json_bool(false));
        return ret;
    }
    const json_t *cd = json_obj_get(agent, "_cache_disabled");
    const json_t *ttl = json_obj_get(agent, "_cache_ttl");
    const json_t *au = json_obj_get(agent, "_cache_ttl_auto");
    json_append(ret, cd ? json_copy(cd) : json_null());
    json_append(ret, ttl ? json_copy(ttl) : json_null());
    json_append(ret, json_bool(mlf_truth(au)));
    return ret;
}

/* ── 4. _with_cache_disabled ────────────────────────────────────────────────
 *   return runtime if cache_disabled is None else {**runtime, "_cache_disabled": cache_disabled}
 */
/* PoP: mlf_with_cache_disabled @ agent/moa_loop.py:_with_cache_disabled */
json_t *mlf_with_cache_disabled(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *runtime = json_obj_get(in, "runtime");
    const json_t *cd = json_obj_get(in, "cache_disabled");
    if (mlf_is_null(cd)) return runtime ? json_copy(runtime) : json_null();
    json_t *out = mlf_is_obj(runtime) ? json_copy(runtime) : json_object();
    json_set(out, "_cache_disabled", json_copy(cd));
    return out;
}

/* ── 5. _hash_messages ──────────────────────────────────────────────────────
 *   sha256("\0".join(f"{m.get('role')}:{m.get('content')}" for m in msgs)
 *          .encode("utf-8", "replace")).hexdigest()
 */
/* PoP: mlf_hash_messages @ agent/moa_loop.py:_hash_messages */
json_t *mlf_hash_messages(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *msgs = json_obj_get(in, "msgs");
    mlf_buf_t b; mlf_buf_init(&b);
    if (mlf_is_arr(msgs)) {
        for (size_t i = 0; i < msgs->c.count; i++) {
            const json_t *m = msgs->c.items[i];
            if (i) mlf_buf_add(&b, "\0", 1);
            char *role = mlf_pystr(json_obj_get(m, "role"));
            char *content = mlf_pystr(json_obj_get(m, "content"));
            mlf_buf_adds(&b, role ? role : "None");
            mlf_buf_adds(&b, ":");
            mlf_buf_adds(&b, content ? content : "None");
            free(role); free(content);
        }
    }
    char hex[65];
    mlf_sha256_hex((const unsigned char *)(b.p ? b.p : ""), b.n, hex);
    free(b.p);
    return json_string(hex);
}

/* ── 6. _tool_activity_since_last_user ──────────────────────────────────────
 *   for msg in reversed(messages):
 *       role = msg.get("role")
 *       if role == "user": return False
 *       if role == "tool" or (role == "assistant" and msg.get("tool_calls")): return True
 *   return False
 */
/* PoP: mlf_tool_activity_since_last_user @ agent/moa_loop.py:_tool_activity_since_last_user */
json_t *mlf_tool_activity_since_last_user(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *msgs = json_obj_get(in, "messages");
    if (mlf_is_arr(msgs)) {
        for (size_t k = msgs->c.count; k > 0; k--) {
            const json_t *m = msgs->c.items[k - 1];
            const char *role = json_get_str(m, "role", "");
            if (mlf_str_eq(role, "user")) return json_bool(false);
            if (mlf_str_eq(role, "tool")
                || (mlf_str_eq(role, "assistant") && mlf_truth(json_obj_get(m, "tool_calls"))))
                return json_bool(true);
        }
    }
    return json_bool(false);
}

/* ── 7. _placeholder_output ─────────────────────────────────────────────────
 *   return _slot_label(slot), note, _RefAccounting(CanonicalUsage())
 */
/* PoP: mlf_placeholder_output @ agent/moa_loop.py:_placeholder_output */
json_t *mlf_placeholder_output(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    json_t *a = json_object();
    mlf_arg_j(a, "slot", json_obj_get(in, "slot"));
    json_t *lbl = mlf_call(io, "_slot_label", a, exc);
    if (*exc) return NULL;
    const json_t *note = json_obj_get(in, "note");
    json_t *ret = json_array();
    json_append(ret, lbl ? lbl : json_null());
    json_append(ret, note ? json_copy(note) : json_null());
    json_append(ret, mlf_acct_fresh(mlf_usage_fresh()));
    return ret;
}

/* ── 8. _join_reference_outputs ─────────────────────────────────────────────
 *   joined = "\n\n".join(f"Reference {idx} — {label}:\n{text}" ...)
 *   if degraded: joined = f"{joined}\n\n{degraded}" if joined else degraded
 *   return joined
 */
/* PoP: mlf_join_reference_outputs @ agent/moa_loop.py:_join_reference_outputs */
json_t *mlf_join_reference_outputs(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *outputs = json_obj_get(in, "outputs");
    const char *degraded = json_get_str(in, "degraded", "");
    mlf_buf_t b; mlf_buf_init(&b);
    if (mlf_is_arr(outputs)) {
        for (size_t i = 0; i < outputs->c.count; i++) {
            const json_t *o = outputs->c.items[i];
            if (i) mlf_buf_adds(&b, "\n\n");
            char idxbuf[32];
            snprintf(idxbuf, sizeof(idxbuf), "%zu", i + 1);
            char *label = mlf_pystr(mlf_out_label(o));
            char *text = mlf_pystr(mlf_out_text(o));
            mlf_buf_adds(&b, "Reference ");
            mlf_buf_adds(&b, idxbuf);
            mlf_buf_adds(&b, " \xe2\x80\x94 ");   /* em dash */
            mlf_buf_adds(&b, label ? label : "None");
            mlf_buf_adds(&b, ":\n");
            mlf_buf_adds(&b, text ? text : "None");
            free(label); free(text);
        }
    }
    if (degraded && degraded[0]) {
        if (b.n) { mlf_buf_adds(&b, "\n\n"); mlf_buf_adds(&b, degraded); }
        else { mlf_buf_adds(&b, degraded); }
    }
    json_t *ret = json_string(b.p ? b.p : "");
    free(b.p);
    return ret;
}

/* ── 9. _guidance_inputs ────────────────────────────────────────────────────
 *   successful = [o for o in reference_outputs if not _is_failed_reference(o[1])]
 *   failed_labels = [label for label, text, _ in reference_outputs if _is_failed_reference(text)]
 *   agg_refs = _redact_reference_outputs(successful) if privacy_full else successful
 *   return agg_refs, _degraded_notice(failed_labels, policy), bool(reference_outputs) and not successful
 */
/* PoP: mlf_guidance_inputs @ agent/moa_loop.py:_guidance_inputs */
json_t *mlf_guidance_inputs(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *outputs = json_obj_get(in, "reference_outputs");
    bool privacy_full = json_get_bool(in, "privacy_full", false);
    const char *policy = json_get_str(in, "policy", "");
    size_t n = mlf_is_arr(outputs) ? outputs->c.count : 0;

    json_t *successful = json_array();
    /* first comprehension: _is_failed_reference once per output */
    for (size_t i = 0; i < n; i++) {
        const json_t *o = outputs->c.items[i];
        json_t *a = json_object();
        mlf_arg_j(a, "text", mlf_out_text(o));
        json_t *r = mlf_call(io, "_is_failed_reference", a, exc);
        if (*exc) { json_free(successful); return NULL; }
        bool failed = mlf_truth(r);
        json_free(r);
        if (!failed) json_append(successful, json_copy(o));
    }
    json_t *failed_labels = json_array();
    /* second comprehension: _is_failed_reference AGAIN per output */
    for (size_t i = 0; i < n; i++) {
        const json_t *o = outputs->c.items[i];
        json_t *a = json_object();
        mlf_arg_j(a, "text", mlf_out_text(o));
        json_t *r = mlf_call(io, "_is_failed_reference", a, exc);
        if (*exc) { json_free(successful); json_free(failed_labels); return NULL; }
        bool failed = mlf_truth(r);
        json_free(r);
        if (failed) {
            const json_t *lbl = mlf_out_label(o);
            json_append(failed_labels, lbl ? json_copy(lbl) : json_null());
        }
    }

    json_t *agg_refs;
    if (privacy_full) {
        json_t *a = json_object();
        mlf_arg_j(a, "reference_outputs", successful);
        agg_refs = mlf_call(io, "_redact_reference_outputs", a, exc);
        if (*exc) { json_free(successful); json_free(failed_labels); return NULL; }
        if (!agg_refs) agg_refs = json_null();
    } else {
        agg_refs = json_copy(successful);
    }

    json_t *da = json_object();
    mlf_arg_j(da, "failed_labels", failed_labels);
    mlf_arg_s(da, "policy", policy);
    json_t *degraded = mlf_call(io, "_degraded_notice", da, exc);
    if (*exc) { json_free(successful); json_free(failed_labels); json_free(agg_refs); return NULL; }

    bool all_failed = mlf_truth(outputs) && successful->c.count == 0;
    json_t *ret = json_array();
    json_append(ret, agg_refs);
    json_append(ret, degraded ? degraded : json_null());
    json_append(ret, json_bool(all_failed));
    json_free(successful); json_free(failed_labels);
    return ret;
}

/* ── 10. _sum_reference_accounting ──────────────────────────────────────────
 *   usage = CanonicalUsage(); cost = None
 *   for _lbl, _txt, acct in outputs:
 *       if isinstance(acct, _RefAccounting):
 *           if isinstance(acct.usage, CanonicalUsage): usage = usage + acct.usage
 *           if acct.cost_usd is not None: cost = (cost or 0) + acct.cost_usd
 *   return usage, cost
 */
/* PoP: mlf_sum_reference_accounting @ agent/moa_loop.py:_sum_reference_accounting */
json_t *mlf_sum_reference_accounting(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *outputs = json_obj_get(in, "outputs");
    json_t *usage = mlf_usage_fresh();
    bool cost_set = false;
    double cost_val = 0.0;
    if (mlf_is_arr(outputs)) {
        for (size_t i = 0; i < outputs->c.count; i++) {
            const json_t *acct = mlf_out_acct(outputs->c.items[i]);
            if (!mlf_is_acct(acct)) continue;
            const json_t *u = json_obj_get(acct, "usage");
            if (mlf_is_usage(u)) {
                json_t *nu = mlf_usage_add(usage, u);
                json_free(usage); usage = nu;
            }
            const json_t *cu = json_obj_get(acct, "cost_usd");
            if (!mlf_is_null(cu)) {
                cost_val = (cost_set ? cost_val : 0.0) + mlf_num(cu, 0.0);
                cost_set = true;
            }
        }
    }
    json_t *ret = json_array();
    json_append(ret, usage);
    json_append(ret, cost_set ? json_number(cost_val) : json_null());
    return ret;
}

/* ── 11. _fold_pending_accounting (facade method) ───────────────────────────
 *   self._pending_reference_usage = (self._pending_reference_usage or CanonicalUsage()) + usage
 *   if cost is not None: self._pending_reference_cost = (self._pending_reference_cost or 0) + cost
 */
/* PoP: mlf_fold_pending_accounting @ agent/moa_loop.py:_fold_pending_accounting */
json_t *mlf_fold_pending_accounting(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)io; (void)exc;
    const json_t *usage = json_obj_get(in, "usage");
    const json_t *pending = json_obj_get(state, "_pending_reference_usage");
    json_t *base = mlf_truth(pending) ? json_copy(pending) : mlf_usage_fresh();
    json_t *nu = mlf_usage_add(base, mlf_is_usage(usage) ? usage : base);
    json_free(base);
    json_set(state, "_pending_reference_usage", nu);
    const json_t *cost = json_obj_get(in, "cost");
    if (!mlf_is_null(cost)) {
        const json_t *pc = json_obj_get(state, "_pending_reference_cost");
        double base_cost = mlf_truth(pc) ? mlf_num(pc, 0.0) : 0.0;
        json_set(state, "_pending_reference_cost", json_number(base_cost + mlf_num(cost, 0.0)));
    }
    return json_null();
}

/* ── 12. _reference_context_length ──────────────────────────────────────────
 *   model = str(slot.get("model") or ""); provider = str(runtime.get("provider") or slot.get("provider") or "")
 *   key = (provider, model); has_cache = isinstance(cache, dict)
 *   if has_cache and key in cache: return cache[key]
 *   try: context_length = get_model_context_length(model=..., base_url=..., api_key=*** provider=...)
 *   except Exception: context_length = None
 *   if has_cache: cache[key] = context_length
 *   return context_length
 */
/* PoP: mlf_reference_context_length @ agent/moa_loop.py:_reference_context_length */
json_t *mlf_reference_context_length(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    const json_t *slot = json_obj_get(in, "slot");
    const json_t *runtime = json_obj_get(in, "runtime");
    json_t *cache = json_obj_get(state, "ctx_cache");
    bool has_cache = mlf_is_obj(cache);

    char *model = mlf_pystr_or_empty(json_obj_get(slot, "model"));
    const json_t *pv = json_obj_get(runtime, "provider");
    if (!mlf_truth(pv)) pv = json_obj_get(slot, "provider");
    char *provider = mlf_pystr_or_empty(pv);

    mlf_buf_t kb; mlf_buf_init(&kb);
    mlf_buf_adds(&kb, provider ? provider : "");
    mlf_buf_adds(&kb, "\t");
    mlf_buf_adds(&kb, model ? model : "");

    if (has_cache && json_has(cache, kb.p ? kb.p : "")) {
        const json_t *cv = json_obj_get(cache, kb.p);
        json_t *ret = cv ? json_copy(cv) : json_null();
        free(model); free(provider); free(kb.p);
        return ret;
    }

    char *base_url = mlf_pystr_or_empty(json_obj_get(runtime, "base_url"));
    char *api_key = mlf_pystr_or_empty(json_obj_get(runtime, "api_key"));
    json_t *a = json_object();
    mlf_arg_s(a, "model", model ? model : "");
    mlf_arg_s(a, "base_url", base_url ? base_url : "");
    mlf_arg_s(a, "api_key", api_key ? api_key : "");
    mlf_arg_s(a, "provider", provider ? provider : "");
    json_t *ctx = mlf_call(io, "get_model_context_length", a, exc);
    if (*exc) {
        if (!mlf_swallow(exc)) {
            free(model); free(provider); free(base_url); free(api_key); free(kb.p);
            return NULL;
        }
        ctx = NULL;   /* except Exception -> None */
    }

    if (has_cache)
        json_set(cache, kb.p ? kb.p : "", ctx ? json_copy(ctx) : json_null());
    json_t *ret = ctx ? ctx : json_null();
    free(model); free(provider); free(base_url); free(api_key); free(kb.p);
    return ret;
}

/* ── 13. _price_reference_response ──────────────────────────────────────────
 *   usage = CanonicalUsage(); raw_usage = getattr(response, "usage", None)
 *   if raw_usage:
 *       with suppress(Exception): usage = normalize_usage(raw_usage, provider=..., api_mode=...)
 *   try:
 *       cost = estimate_usage_cost(slot.get("model") or "", usage, provider=..., base_url=..., api_key=***
 *       return usage, cost.amount_usd, cost.status, cost.source
 *   except Exception: return usage, None, None, None
 */
/* PoP: mlf_price_reference_response @ agent/moa_loop.py:_price_reference_response */
json_t *mlf_price_reference_response(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const json_t *slot = json_obj_get(in, "slot");
    const json_t *runtime = json_obj_get(in, "runtime");
    const json_t *raw_usage = json_obj_get(in, "response_usage");
    json_t *usage = mlf_usage_fresh();

    if (mlf_truth(raw_usage)) {
        json_t *a = json_object();
        mlf_arg_j(a, "raw_usage", raw_usage);
        mlf_arg_j(a, "provider", json_obj_get(runtime, "provider"));
        mlf_arg_j(a, "api_mode", json_obj_get(runtime, "api_mode"));
        json_t *u = mlf_call(io, "normalize_usage", a, exc);
        if (*exc) { if (!mlf_swallow(exc)) { json_free(usage); return NULL; } }
        else if (u) { json_free(usage); usage = u; }
    }

    char *model = mlf_pystr_or_empty(json_obj_get(slot, "model"));
    json_t *a = json_object();
    mlf_arg_s(a, "model", model ? model : "");
    mlf_arg_j(a, "usage", usage);
    mlf_arg_j(a, "provider", json_obj_get(runtime, "provider"));
    mlf_arg_j(a, "base_url", json_obj_get(runtime, "base_url"));
    mlf_arg_j(a, "api_key", json_obj_get(runtime, "api_key"));
    json_t *cost = mlf_call(io, "estimate_usage_cost", a, exc);
    free(model);
    if (*exc) {
        if (!mlf_swallow(exc)) { json_free(usage); return NULL; }
        json_t *ret = json_array();
        json_append(ret, usage);
        json_append(ret, json_null());
        json_append(ret, json_null());
        json_append(ret, json_null());
        return ret;
    }
    json_t *ret = json_array();
    json_append(ret, usage);
    const json_t *amt = json_obj_get(cost, "amount_usd");
    const json_t *st = json_obj_get(cost, "status");
    const json_t *src = json_obj_get(cost, "source");
    json_append(ret, amt ? json_copy(amt) : json_null());
    json_append(ret, st ? json_copy(st) : json_null());
    json_append(ret, src ? json_copy(src) : json_null());
    json_free(cost);
    return ret;
}

/* ── 14. _record_late ───────────────────────────────────────────────────────
 *   try: sink(label, future.result()[2])
 *   except Exception: logger.debug(...)   (silent)
 */
/* PoP: mlf_record_late @ agent/moa_loop.py:_record_late */
json_t *mlf_record_late(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    (void)state;
    const char *label = json_get_str(in, "label", "");
    double idx = json_get_num(in, "future_idx", 0);

    json_t *fa = json_object();
    mlf_arg_n(fa, "idx", idx);
    json_t *out = mlf_call(io, "future.result", fa, exc);
    if (*exc) {
        if (!mlf_swallow(exc)) { json_free(out); return NULL; }
        json_free(out); return json_null();
    }

    const json_t *acct = mlf_out_acct(out);
    json_t *sa = json_object();
    mlf_arg_s(sa, "label", label);
    mlf_arg_j(sa, "acct", acct);
    json_t *sr = mlf_call(io, "sink", sa, exc);
    if (*exc) { if (!mlf_swallow(exc)) { json_free(sr); json_free(out); return NULL; } }
    json_free(sr); json_free(out);
    return json_null();
}

/* ── 15. _settle_interrupted ────────────────────────────────────────────────
 *   for future, idx in futures.items():
 *       if results[idx] is not None: continue
 *       cancelled = future.cancel()
 *       if not cancelled and future.done(): results[idx] = future.result(); continue
 *       results[idx] = _placeholder_output(reference_models[idx], _INTERRUPTED_REFERENCE_NOTE)
 *       if not cancelled and late_accounting_sink is not None:
 *           future.add_done_callback(partial(_record_late, sink, results[idx][0]))
 */
/* PoP: mlf_settle_interrupted @ agent/moa_loop.py:_settle_interrupted */
json_t *mlf_settle_interrupted(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    const json_t *futures = json_obj_get(in, "futures");
    const json_t *ref_models = json_obj_get(in, "reference_models");
    bool sink_present = json_get_bool(in, "sink_present", false);
    json_t *results = json_obj_get(state, "results");
    if (!mlf_is_arr(results)) { results = json_array(); json_set(state, "results", results); }

    if (mlf_is_arr(futures)) {
        for (size_t i = 0; i < futures->c.count; i++) {
            const json_t *fspec = futures->c.items[i];
            double idx = json_get_num(fspec, "idx", 0);
            size_t ix = (size_t)idx;
            const json_t *cur = json_get(results, ix);
            if (!mlf_is_null(cur)) continue;   /* already finished */

            json_t *ca = json_object(); mlf_arg_n(ca, "idx", idx);
            json_t *cr = mlf_call(io, "future.cancel", ca, exc);
            if (*exc) return NULL;
            bool cancelled = mlf_truth(cr);
            json_free(cr);

            bool settled = false;
            if (!cancelled) {
                json_t *da = json_object(); mlf_arg_n(da, "idx", idx);
                json_t *dr = mlf_call(io, "future.done", da, exc);
                if (*exc) return NULL;
                bool done = mlf_truth(dr);
                json_free(dr);
                if (done) {
                    json_t *ra = json_object(); mlf_arg_n(ra, "idx", idx);
                    json_t *res = mlf_call(io, "future.result", ra, exc);
                    if (*exc) return NULL;
                    /* results[idx] = res */
                    while (results->c.count <= ix) json_append(results, json_null());
                    json_free(results->c.items[ix]);
                    results->c.items[ix] = res ? res : json_null();
                    settled = true;
                }
            }
            if (settled) continue;

            json_t *pa = json_object();
            mlf_arg_j(pa, "slot", mlf_is_arr(ref_models) ? json_get(ref_models, ix) : NULL);
            mlf_arg_s(pa, "note", MLF_INTERRUPTED_NOTE);
            json_t *ph = mlf_call(io, "_placeholder_output", pa, exc);
            if (*exc) return NULL;
            while (results->c.count <= ix) json_append(results, json_null());
            json_free(results->c.items[ix]);
            results->c.items[ix] = ph ? ph : json_null();

            if (!cancelled && sink_present) {
                const json_t *lbl = mlf_out_label(ph);
                char *ls = mlf_pystr(lbl);
                json_t *aa = json_object();
                mlf_arg_n(aa, "idx", idx);
                mlf_arg_s(aa, "label", ls ? ls : "");
                free(ls);
                json_t *ar = mlf_call(io, "future.add_done_callback", aa, exc);
                if (*exc) return NULL;
                json_free(ar);
            }
        }
    }
    return json_null();
}

/* ── 16. _fanout_cache_key (facade method) ──────────────────────────────────
 *   Cadence decision chain: user_turn / per_iteration / every_n:<N>.
 */
/* PoP: mlf_fanout_cache_key @ agent/moa_loop.py:_fanout_cache_key */
json_t *mlf_fanout_cache_key(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    const json_t *preset = json_obj_get(in, "preset");
    const json_t *ref_messages = json_obj_get(in, "ref_messages");
    const json_t *ref_models = json_obj_get(in, "reference_models");
    size_t nmsg = mlf_is_arr(ref_messages) ? ref_messages->c.count : 0;

    /* fanout_mode = str(preset.get("fanout") or "user_turn").strip().lower() */
    const json_t *fv = json_obj_get(preset, "fanout");
    char *fs = mlf_pystr(mlf_truth(fv) ? fv : NULL);
    if (!mlf_truth(fv)) { free(fs); fs = strdup("user_turn"); }
    char *stripped = mlf_strip(fs); free(fs);
    char *fanout_mode = mlf_lower_dup(stripped); free(stripped);

    long long every_n = 0;
    if (strncmp(fanout_mode, "every_n:", 8) == 0) {
        /* int(fanout_mode.split(":", 1)[1]) with suppress(TypeError, ValueError) */
        const char *p = fanout_mode + 8;
        while (*p == ' ' || (*p >= 0x09 && *p <= 0x0D)) p++;
        bool neg = false;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        const char *d = p;
        long long val = 0; bool any = false;
        while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; any = true; }
        while (*p == ' ' || (*p >= 0x09 && *p <= 0x0D)) p++;
        if (any && *p == '\0') every_n = neg ? -val : val;   /* else ValueError -> 0 */
        (void)d;
        if (every_n < 2) { free(fanout_mode); fanout_mode = strdup("per_iteration"); }
    }

    /* sig_messages = turn_prefix = ref_messages */
    size_t prefix_len = nmsg;          /* turn_prefix end (exclusive) */
    size_t sig_len = nmsg;             /* sig_messages end */
    bool sig_is_prefix = false;
    bool user_turn = mlf_str_eq(fanout_mode, "user_turn");
    if (user_turn || every_n >= 2) {
        /* last REAL user message (content != _ADVISORY_INSTRUCTION) */
        long long last_user = -1;
        for (size_t k = nmsg; k > 0; k--) {
            const json_t *m = ref_messages->c.items[k - 1];
            if (mlf_str_eq(json_get_str(m, "role", ""), "user")) {
                const json_t *content = json_obj_get(m, "content");
                const char *cs = mlf_str(content);
                if (!(cs && mlf_str_eq(cs, MLF_ADVISORY_INSTRUCTION))) {
                    last_user = (long long)(k - 1);
                    break;
                }
            }
        }
        if (last_user >= 0) prefix_len = (size_t)last_user + 1;
        if (user_turn) { sig_len = prefix_len; sig_is_prefix = true; }
    }
    (void)sig_is_prefix;

    if (every_n >= 2) {
        json_t *tp = json_array();
        for (size_t i = 0; i < prefix_len; i++) json_append(tp, json_copy(ref_messages->c.items[i]));
        json_t *ha = json_object(); mlf_arg_j(ha, "msgs", tp); json_free(tp);
        json_t *turn_sig = mlf_call(io, "_hash_messages", ha, exc);
        if (*exc) { free(fanout_mode); return NULL; }
        const json_t *prev_turn = json_obj_get(state, "_fanout_turn_sig");
        char *ts = mlf_pystr(turn_sig);
        char *ps = mlf_pystr(prev_turn);
        bool turn_changed = !mlf_str_eq(ts, ps);
        free(ts); free(ps);
        if (turn_changed) {
            json_set(state, "_fanout_turn_sig", turn_sig ? json_copy(turn_sig) : json_null());
            json_set(state, "_fanout_iteration_count", json_number(0));
            json_set(state, "_fanout_last_state_sig", json_null());
        }
        json_free(turn_sig);

        json_t *sa = json_object(); mlf_arg_j(sa, "msgs", ref_messages);
        json_t *state_sig = mlf_call(io, "_hash_messages", sa, exc);
        if (*exc) { free(fanout_mode); return NULL; }
        const json_t *prev_state = json_obj_get(state, "_fanout_last_state_sig");
        char *ss = mlf_pystr(state_sig);
        char *ps2 = mlf_pystr(prev_state);
        bool state_changed = !mlf_str_eq(ss, ps2);
        free(ss); free(ps2);
        long long count = (long long)json_get_num(state, "_fanout_iteration_count", 0);
        if (state_changed) {
            json_set(state, "_fanout_last_state_sig", state_sig ? json_copy(state_sig) : json_null());
            count += 1;
            json_set(state, "_fanout_iteration_count", json_number((double)count));
        }
        json_free(state_sig);

        const json_t *cache_outputs = json_obj_get(state, "_ref_cache_outputs");
        if (mlf_py_mod(count - 1, every_n) != 0 && mlf_truth(cache_outputs)) {
            const json_t *pinned = json_obj_get(state, "_ref_cache_key");
            json_t *ret = pinned ? json_copy(pinned) : json_null();
            free(fanout_mode);
            return ret;
        }
    }

    /* final key: (preset_name, _hash_messages(sig_messages), tuple(_slot_label(s) ...)) */
    json_t *sm = json_array();
    for (size_t i = 0; i < sig_len; i++) json_append(sm, json_copy(ref_messages->c.items[i]));
    json_t *ha = json_object(); mlf_arg_j(ha, "msgs", sm); json_free(sm);
    json_t *hash = mlf_call(io, "_hash_messages", ha, exc);
    if (*exc) { free(fanout_mode); return NULL; }

    json_t *labels = json_array();
    if (mlf_is_arr(ref_models)) {
        for (size_t i = 0; i < ref_models->c.count; i++) {
            json_t *la = json_object(); mlf_arg_j(la, "slot", ref_models->c.items[i]);
            json_t *lbl = mlf_call(io, "_slot_label", la, exc);
            if (*exc) { free(fanout_mode); json_free(hash); json_free(labels); return NULL; }
            json_append(labels, lbl ? lbl : json_null());
        }
    }
    const json_t *pn = json_obj_get(state, "preset_name");
    json_t *ret = json_array();
    json_append(ret, pn ? json_copy(pn) : json_null());
    json_append(ret, hash ? hash : json_null());
    json_append(ret, labels);
    free(fanout_mode);
    return ret;
}

/* ── 17. _build_guidance (facade method) ────────────────────────────────────
 *   agg_refs, degraded, all_failed = _guidance_inputs(...)
 *   header = ...; if all_failed: (degraded ? header+notice : None)
 *   if agg_refs or degraded: header + References + stale note + joined
 *   return None
 */
/* PoP: mlf_build_guidance @ agent/moa_loop.py:_build_guidance */
json_t *mlf_build_guidance(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    const json_t *outputs = json_obj_get(in, "reference_outputs");
    const json_t *aggregator = json_obj_get(in, "aggregator");
    const char *policy = json_get_str(in, "degraded_reference_policy", "loud");
    bool stale = json_get_bool(in, "stale", false);
    const char *privacy = json_get_str(state, "_privacy_mode", "");
    bool privacy_full = mlf_str_eq(privacy, "full");

    json_t *ga = json_object();
    mlf_arg_j(ga, "reference_outputs", outputs);
    mlf_arg_b(ga, "privacy_full", privacy_full);
    mlf_arg_s(ga, "policy", policy);
    json_t *gi = mlf_call(io, "_guidance_inputs", ga, exc);
    if (*exc) return NULL;
    const json_t *agg_refs = json_get(gi, 0);
    const json_t *degraded = json_get(gi, 1);
    bool all_failed = mlf_truth(json_get(gi, 2));
    const char *degraded_s = mlf_str(degraded);
    bool degraded_truthy = mlf_truth(degraded);

    json_t *la = json_object(); mlf_arg_j(la, "slot", aggregator);
    json_t *agg_label = mlf_call(io, "_slot_label", la, exc);
    if (*exc) { json_free(gi); return NULL; }
    char *agg_ls = mlf_pystr(agg_label);

    const char *preset_name = json_get_str(state, "preset_name", "");
    mlf_buf_t header; mlf_buf_init(&header);
    mlf_buf_adds(&header, "[Mixture of Agents reference context]\nPreset: ");
    mlf_buf_adds(&header, preset_name);
    mlf_buf_adds(&header, "\nAggregator/acting model: ");
    mlf_buf_adds(&header, agg_ls ? agg_ls : "None");
    mlf_buf_adds(&header, "\n");

    json_t *ret = NULL;
    if (all_failed) {
        if (degraded_truthy) {
            mlf_buf_t b; mlf_buf_init(&b);
            mlf_buf_adds(&b, header.p ? header.p : "");
            mlf_buf_adds(&b, "\nAll reference models failed this turn \xe2\x80\x94 no advisory "
                             "guidance is available. Act on your own judgment.\n\n");
            mlf_buf_adds(&b, degraded_s ? degraded_s : "");
            ret = json_string(b.p ? b.p : "");
            free(b.p);
        } else {
            ret = json_null();
        }
    } else if (mlf_truth(agg_refs) || degraded_truthy) {
        /* References: join of agg_refs labels */
        mlf_buf_t refs; mlf_buf_init(&refs);
        if (mlf_is_arr(agg_refs)) {
            for (size_t i = 0; i < agg_refs->c.count; i++) {
                if (i) mlf_buf_adds(&refs, ", ");
                char *ls = mlf_pystr(mlf_out_label(agg_refs->c.items[i]));
                mlf_buf_adds(&refs, ls ? ls : "None");
                free(ls);
            }
        }
        json_t *ja = json_object();
        mlf_arg_j(ja, "outputs", agg_refs);
        mlf_arg_s(ja, "degraded", degraded_s ? degraded_s : "");
        json_t *joined = mlf_call(io, "_join_reference_outputs", ja, exc);
        if (*exc) { free(refs.p); free(header.p); free(agg_ls); json_free(gi); return NULL; }
        char *js = mlf_pystr(joined);
        mlf_buf_t b; mlf_buf_init(&b);
        mlf_buf_adds(&b, header.p ? header.p : "");
        mlf_buf_adds(&b, "References: ");
        mlf_buf_adds(&b, refs.p ? refs.p : "");
        mlf_buf_adds(&b, "\n\nUse the reference responses below as private context. You are the "
                         "aggregator and acting model: answer the user directly or call tools as needed.\n");
        mlf_buf_adds(&b, stale ? MLF_STALE_GUIDANCE_NOTE : "");
        mlf_buf_adds(&b, "\n");
        mlf_buf_adds(&b, js ? js : "None");
        ret = json_string(b.p ? b.p : "");
        free(b.p); free(js); json_free(joined); free(refs.p);
    } else {
        ret = json_null();
    }
    free(header.p); free(agg_ls); json_free(agg_label); json_free(gi);
    return ret;
}

/* ── 18. _run_fanout (facade method) ────────────────────────────────────────
 *   Cache-MISS fan-out orchestration: run advisors, account, trace, metrics,
 *   emit references / phase / aggregating in the live ORDER.
 */
/* PoP: mlf_run_fanout @ agent/moa_loop.py:_run_fanout */
json_t *mlf_run_fanout(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc) {
    const json_t *preset = json_obj_get(in, "preset");
    const json_t *ref_messages = json_obj_get(in, "ref_messages");
    const json_t *ref_models = json_obj_get(in, "reference_models");
    const json_t *aggregator = json_obj_get(in, "aggregator");
    const json_t *agg_temp = json_obj_get(in, "aggregator_temperature");
    const json_t *cache_key = json_obj_get(in, "cache_key");

    /* temperature = _preset_temperature(preset, "reference_temperature") */
    json_t *ta = json_object();
    mlf_arg_j(ta, "preset", preset);
    mlf_arg_s(ta, "key", "reference_temperature");
    json_t *temp = mlf_call(io, "_preset_temperature", ta, exc);
    if (*exc) return NULL;

    /* reference_timeout = float(raw) if raw else None */
    const json_t *raw_to = json_obj_get(preset, "reference_timeout");
    json_t *ref_timeout = mlf_truth(raw_to) ? json_number(mlf_to_double(raw_to)) : json_null();

    /* reference_outputs = _run_references_parallel(...) — the stub fires the
     * scripted progress callbacks (each an _emit seam call) before returning. */
    json_t *ra = json_object();
    mlf_arg_j(ra, "reference_models", ref_models);
    mlf_arg_j(ra, "ref_messages", ref_messages);
    mlf_arg_j(ra, "temperature", temp);
    mlf_arg_j(ra, "reference_timeout", ref_timeout);
    mlf_arg_j(ra, "agent", json_obj_get(state, "_agent"));
    mlf_arg_s(ra, "progress_callback", "<callable>");
    mlf_arg_s(ra, "late_accounting_sink", "<callable>");
    json_free(temp); json_free(ref_timeout);
    json_t *outputs = mlf_call(io, "_run_references_parallel", ra, exc);
    if (*exc) return NULL;
    if (!mlf_is_arr(outputs)) outputs = json_array();
    size_t nout = outputs->c.count;

    /* interrupted = any(text == _INTERRUPTED_REFERENCE_NOTE ...) */
    bool interrupted = false;
    for (size_t i = 0; i < nout; i++) {
        const char *t = mlf_str(mlf_out_text(outputs->c.items[i]));
        if (t && mlf_str_eq(t, MLF_INTERRUPTED_NOTE)) { interrupted = true; break; }
    }
    json_set(state, "_ref_cache_key", interrupted ? json_null() : (cache_key ? json_copy(cache_key) : json_null()));
    json_set(state, "_ref_cache_outputs", interrupted ? json_array() : json_copy(outputs));

    /* self._fold_pending_accounting(*_sum_reference_accounting(reference_outputs)) */
    json_t *sa = json_object(); mlf_arg_j(sa, "outputs", outputs);
    json_t *sum = mlf_call(io, "_sum_reference_accounting", sa, exc);
    if (*exc) { json_free(outputs); return NULL; }
    json_t *fa = json_object();
    mlf_arg_j(fa, "usage", json_get(sum, 0));
    mlf_arg_j(fa, "cost", json_get(sum, 1));
    json_t *fr = mlf_call(io, "_fold_pending_accounting", fa, exc);
    json_free(fr); json_free(sum);
    if (*exc) { json_free(outputs); return NULL; }

    /* trace_refs (privacy-redacted copy) */
    const char *privacy = json_get_str(state, "_privacy_mode", "");
    bool privacy_on = mlf_truth(json_obj_get(state, "_privacy_mode"));
    (void)privacy;
    json_t *trace_refs = json_array();
    if (privacy_on) {
        for (size_t i = 0; i < nout; i++) {
            const json_t *o = outputs->c.items[i];
            json_t *rta = json_object(); mlf_arg_j(rta, "text", mlf_out_text(o));
            json_t *rt = mlf_call(io, "_redact_reference_text", rta, exc);
            if (*exc) { json_free(trace_refs); json_free(outputs); return NULL; }
            json_t *raa = json_object(); mlf_arg_j(raa, "acct", mlf_out_acct(o));
            json_t *racct = mlf_call(io, "_redact_trace_accounting", raa, exc);
            if (*exc) { json_free(rt); json_free(trace_refs); json_free(outputs); return NULL; }
            json_t *tr = json_array();
            json_append(tr, json_copy(mlf_out_label(o)));
            json_append(tr, rt ? rt : json_null());
            json_append(tr, racct ? racct : json_null());
            json_append(trace_refs, tr);
        }
    } else {
        json_free(trace_refs);
        trace_refs = json_copy(outputs);
    }

    /* self._pending_trace = {...} — all four keys always present (dict literal) */
    json_t *pt = json_object();
    const json_t *pn = json_obj_get(state, "preset_name");
    json_set(pt, "preset", pn ? json_copy(pn) : json_null());
    json_set(pt, "reference_outputs", json_copy(trace_refs));
    json_set(pt, "aggregator_slot", aggregator ? json_copy(aggregator) : json_null());
    json_set(pt, "aggregator_temperature", agg_temp ? json_copy(agg_temp) : json_null());
    json_set(state, "_pending_trace", pt);

    /* self._last_reference_metrics = [slot_metrics(acct, label, output=text) ...] */
    json_t *metrics = json_array();
    bool metrics_failed = false;
    for (size_t i = 0; i < trace_refs->c.count; i++) {
        const json_t *tr = trace_refs->c.items[i];
        json_t *ma = json_object();
        mlf_arg_j(ma, "acct", mlf_out_acct(tr));
        mlf_arg_j(ma, "label", mlf_out_label(tr));
        mlf_arg_j(ma, "output", mlf_out_text(tr));
        json_t *sm = mlf_call(io, "slot_metrics", ma, exc);
        if (*exc) {
            if (!mlf_swallow(exc)) {
                json_free(metrics); json_free(trace_refs); json_free(outputs);
                return NULL;
            }
            metrics_failed = true; break;
        }
        json_append(metrics, sm ? sm : json_null());
    }
    json_set(state, "_last_reference_metrics", metrics_failed ? json_null() : metrics);

    /* emit each reference BEFORE the aggregator acts */
    for (size_t i = 0; i < nout; i++) {
        const json_t *o = outputs->c.items[i];
        const json_t *text = mlf_out_text(o);
        json_t *emit_text = NULL;
        if (privacy_on) {
            json_t *rta = json_object(); mlf_arg_j(rta, "text", text);
            emit_text = mlf_call(io, "_redact_reference_text", rta, exc);
            if (*exc) { json_free(trace_refs); json_free(outputs); return NULL; }
        }
        json_t *ea = json_object();
        mlf_arg_s(ea, "event", "moa.reference");
        mlf_arg_n(ea, "index", (double)(i + 1));
        mlf_arg_n(ea, "count", (double)nout);
        mlf_arg_j(ea, "label", mlf_out_label(o));
        mlf_arg_j(ea, "text", privacy_on ? emit_text : text);
        json_free(emit_text);
        json_t *er = mlf_call(io, "_emit", ea, exc);
        json_free(er);
        if (*exc) { json_free(trace_refs); json_free(outputs); return NULL; }
    }
    if (nout > 0) {
        json_t *ala = json_object(); mlf_arg_j(ala, "slot", aggregator);
        json_t *agg_label = mlf_call(io, "_slot_label", ala, exc);
        if (*exc) { json_free(trace_refs); json_free(outputs); return NULL; }
        json_t *p1 = json_object();
        mlf_arg_s(p1, "event", "moa.phase");
        mlf_arg_s(p1, "phase", "aggregator");
        mlf_arg_n(p1, "refs_done", (double)nout);
        mlf_arg_n(p1, "refs_total", (double)nout);
        mlf_arg_j(p1, "aggregator", agg_label);
        json_t *r1 = mlf_call(io, "_emit", p1, exc); json_free(r1);
        if (*exc) { json_free(agg_label); json_free(trace_refs); json_free(outputs); return NULL; }
        json_t *p2 = json_object();
        mlf_arg_s(p2, "event", "moa.aggregating");
        mlf_arg_j(p2, "aggregator", agg_label);
        mlf_arg_n(p2, "ref_count", (double)nout);
        json_t *r2 = mlf_call(io, "_emit", p2, exc); json_free(r2);
        json_free(agg_label);
        if (*exc) { json_free(trace_refs); json_free(outputs); return NULL; }
    }
    json_free(trace_refs);
    return outputs;   /* reference_outputs (ownership transfers to caller) */
}