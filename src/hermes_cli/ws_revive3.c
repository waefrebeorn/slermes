/*
 * ws_revive3.c — deep-stub revival group 3: faithful C ports of 27
 * hermes_cli web-server helpers/endpoints formerly stubbed in
 * web_server_wrappers.c (PoP tags moved here with the definitions).
 *
 * Signatures keep the original stub shape: json_t *fn(json_t *req).
 * req carries the call's arguments as named JSON fields; the function
 * mutates req in place where the live code mutates module state, and
 * returns the live return value as JSON (NULL pointer == Python None
 * is emitted as a JSON null node; {"__raise__": ...} == an exception).
 *
 * I/O rides one injected universal seam channel (g_w3_io, installed
 * per case by the harness that #includes this file):
 *   json_t *call(io, name, args) — args is ADOPTED by the seam (the
 *   caller must NOT free it); the result is a NEW json_t owned by the
 *   caller. NULL result == None; {"__raise__": cls, "message": msg}
 *   == a raised exception. No channel installed == fail-closed
 *   RuntimeError. w3_seam records {ev, args, ret} in io->trace.
 *
 * Modeling conventions (identical on the oracle side):
 * - Integral JSON numbers model Python ints, fractional ones floats;
 *   fixtures never carry integral float literals (123.0 reads as 123).
 * - JSON arrays model lists/tuples; objects model dicts or attribute
 *   carriers (SimpleNamespace) where the live code reads attributes.
 * - frozenset results normalize to SORTED arrays.
 * - HTTPException rides {"__raise__":"HTTPException","status_code":N,
 *   "detail":...}; every other exception {"__raise__":cls,"message":str}.
 * - except Exception sites pass SystemExit/KeyboardInterrupt/
 *   GeneratorExit through (w3_is_base_exc).
 * - str(value) fixture scope: scalars only (non-empty containers would
 *   need Python repr); .lower()/regex \D fixture scope: ASCII only.
 * - sys.platform is pinned to "win32" (the port targets Windows).
 * - sha1 is implemented locally so the unit links libjson alone.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "hermes_json.h"

typedef struct w3_io {
    json_t *(*call)(struct w3_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} w3_io_t;

/* Injected per case by the harness; NULL = fail-closed production default. */
static w3_io_t *g_w3_io;

static char *w3_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static json_t *w3_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

static json_t *w3_http_exc(int status, const char *detail) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "HTTPException");
    json_set(e, "status_code", json_number((double)status));
    json_set_hdr(e, "detail", detail ? detail : "");
    return e;
}

static int w3_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int w3_is_base_exc(const json_t *r) {
    if (!w3_is_raise(r)) return 0;
    const char *k = json_get_str(r, "__raise__", "");
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0 ||
           strcmp(k, "GeneratorExit") == 0;
}

static int w3_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static const char *w3_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *w3_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* Universal seam: args ADOPTED (never freed by the caller); ret caller-owned. */
static json_t *w3_seam(const char *name, json_t *args) {
    if (!g_w3_io || !g_w3_io->call) {
        if (args) json_free(args);
        char msg[192];
        snprintf(msg, sizeof msg, "no seam installed for %s", name);
        return w3_raise("RuntimeError", msg);
    }
    json_t *ret = g_w3_io->call(g_w3_io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    if (g_w3_io->trace) json_append(g_w3_io->trace, ev);
    else json_free(ev);
    return ret;
}

/* ── growable string buffer ── */
typedef struct { char *b; size_t len, cap; } w3_buf;

static void w3_bneed(w3_buf *s, size_t extra) {
    if (s->len + extra + 1 <= s->cap) return;
    size_t nc = s->cap ? s->cap : 64;
    while (nc < s->len + extra + 1) nc *= 2;
    char *nb = realloc(s->b, nc);
    if (!nb) return;
    s->b = nb; s->cap = nc;
}
static void w3_bput(w3_buf *s, const char *add, size_t n) {
    w3_bneed(s, n);
    if (!s->b) return;
    memcpy(s->b + s->len, add, n);
    s->len += n;
    s->b[s->len] = '\0';
}
static void w3_bputs(w3_buf *s, const char *add) { w3_bput(s, add, strlen(add)); }
static void w3_bputc(w3_buf *s, char c) { w3_bput(s, &c, 1); }
static char *w3_bdone(w3_buf *s) {
    if (!s->b) return w3_strdup("");
    char *r = s->b;
    s->b = NULL; s->len = s->cap = 0;
    return r;
}

/* ── UTF-8 + Python str helpers ── */
static unsigned w3_cp(const char *s, size_t *adv) {
    const unsigned char *p = (const unsigned char *)s;
    unsigned c = p[0];
    if (c < 0x80) { *adv = 1; return c; }
    if ((c & 0xE0) == 0xC0 && p[1] && (p[1] & 0xC0) == 0x80) {
        *adv = 2; return ((c & 0x1F) << 6) | (p[1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && p[1] && p[2] &&
        (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *adv = 3; return ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && p[1] && p[2] && p[3] &&
        (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        *adv = 4;
        return ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    }
    *adv = 1; return 0xFFFD;
}

/* Python str.strip() White_Space set (full Unicode, not just ASCII). */
static int w3_is_space_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    return 0;
}

static char *w3_strip(const char *s) {
    if (!s) return w3_strdup("");
    size_t n = strlen(s), start = 0, end = n;
    while (start < n) {
        size_t adv; unsigned cp = w3_cp(s + start, &adv);
        if (!w3_is_space_cp(cp)) break;
        start += adv;
    }
    while (end > start) {
        size_t bs = end - 1;
        while (bs > start && ((unsigned char)s[bs] & 0xC0) == 0x80) bs--;
        size_t adv; unsigned cp = w3_cp(s + bs, &adv);
        if (bs + adv != end || !w3_is_space_cp(cp)) break;
        end = bs;
    }
    char *r = malloc(end - start + 1);
    if (!r) return NULL;
    memcpy(r, s + start, end - start);
    r[end - start] = '\0';
    return r;
}

/* ASCII lower (fixture scope for .lower() is ASCII; documented in header). */
static char *w3_lower_ascii(const char *s) {
    char *r = w3_strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    return r;
}

/* Python float repr: shortest round-trip digits, fixed notation for
 * -4 <= exp < 16 (integral fixed values keep ".0"), else e+XX/e-XX. */
static char *w3_float_repr(double v) {
    if (isnan(v)) return w3_strdup("nan");
    if (isinf(v)) return w3_strdup(v < 0 ? "-inf" : "inf");
    char buf[64];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*e", prec - 1, v);
        if (strtod(buf, NULL) == v) break;
    }
    const char *p = buf;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    char digits[24]; int nd = 0;
    digits[nd++] = *p++;
    if (*p == '.') {
        p++;
        while (*p && *p != 'e' && *p != 'E' && nd < 22) digits[nd++] = *p++;
    }
    digits[nd] = '\0';
    int exp = (*p == 'e' || *p == 'E') ? atoi(p + 1) : 0;
    w3_buf out = {0};
    if (neg) w3_bputc(&out, '-');
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            int intlen = exp + 1;
            for (int i = 0; i < intlen; i++) w3_bputc(&out, i < nd ? digits[i] : '0');
            if (nd > intlen) {
                w3_bputc(&out, '.');
                for (int i = intlen; i < nd; i++) w3_bputc(&out, digits[i]);
            } else {
                w3_bputs(&out, ".0");
            }
        } else {
            w3_bputs(&out, "0.");
            for (int i = 0; i < -exp - 1; i++) w3_bputc(&out, '0');
            for (int i = 0; i < nd; i++) w3_bputc(&out, digits[i]);
        }
    } else {
        w3_bputc(&out, digits[0]);
        if (nd > 1) {
            w3_bputc(&out, '.');
            for (int i = 1; i < nd; i++) w3_bputc(&out, digits[i]);
        }
        w3_bputc(&out, 'e');
        w3_bputc(&out, exp < 0 ? '-' : '+');
        char eb[8];
        snprintf(eb, sizeof eb, "%02d", exp < 0 ? -exp : exp);
        w3_bputs(&out, eb);
    }
    return w3_bdone(&out);
}

/* Python str() for scalar JSON values (containers excluded by fixture scope). */
static char *w3_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return w3_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return w3_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d) {
                char b[40];
                snprintf(b, sizeof b, "%.0f", d);
                return w3_strdup(b);
            }
            return w3_float_repr(d);
        }
        case JSON_STRING: return w3_strdup(v->str_val ? v->str_val : "");
        default: return w3_strdup("");
    }
}

/* str(value or "") — the full falsy set collapses to the empty string. */
static char *w3_py_str_or_empty(const json_t *v) {
    if (!w3_truthy(v)) return w3_strdup("");
    return w3_py_str(v);
}

/* Deep JSON equality (Python == on JSON-shaped values; bool/int identity
 * edge excluded by fixture scope). */
static int w3_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!w3_json_eq(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !w3_json_eq(a->c.items[i], bv)) return 0;
            }
            return 1;
        default: return 0;
    }
}
/* ── local SHA-1 (keeps the unit libjson-only; no libhash link) ── */
static char *w3_sha1_hex(const unsigned char *data, size_t len) {
    uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu,
             h3 = 0x10325476u, h4 = 0xC3D2E1F0u;
    size_t total = ((len + 8) / 64 + 1) * 64;
    unsigned char *msg = calloc(total, 1);
    if (!msg) return NULL;
    if (len) memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) msg[total - 1 - i] = (unsigned char)(bits >> (8 * i));
    for (size_t off = 0; off < total; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[off + 4 * i] << 24) | ((uint32_t)msg[off + 4 * i + 1] << 16) |
                   ((uint32_t)msg[off + 4 * i + 2] << 8) | (uint32_t)msg[off + 4 * i + 3];
        for (int i = 16; i < 80; i++) {
            uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (x << 1) | (x >> 31);
        }
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;             k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;             k = 0xCA62C1D6u; }
            uint32_t t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = t;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    free(msg);
    char *hex = malloc(41);
    if (!hex) return NULL;
    snprintf(hex, 41, "%08x%08x%08x%08x%08x", h0, h1, h2, h3, h4);
    return hex;
}

/* ── json.dumps(value) with Python defaults: ", "/": " separators,
 * ensure_ascii=True (non-ASCII -> \uXXXX, astral -> surrogate pair) ── */
static void w3_dumps_str(const char *s, w3_buf *out) {
    w3_bputc(out, '"');
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) {
        unsigned char c = *p;
        if (c == '"')       { w3_bputs(out, "\\\""); p++; }
        else if (c == '\\') { w3_bputs(out, "\\\\"); p++; }
        else if (c == '\n') { w3_bputs(out, "\\n"); p++; }
        else if (c == '\r') { w3_bputs(out, "\\r"); p++; }
        else if (c == '\t') { w3_bputs(out, "\\t"); p++; }
        else if (c == '\b') { w3_bputs(out, "\\b"); p++; }
        else if (c == '\f') { w3_bputs(out, "\\f"); p++; }
        else if (c < 0x20)  { char u[8]; snprintf(u, sizeof u, "\\u%04x", c); w3_bputs(out, u); p++; }
        else if (c < 0x80)  { w3_bputc(out, (char)c); p++; }
        else {
            size_t adv; unsigned cp = w3_cp((const char *)p, &adv);
            char u[16];
            if (cp > 0xFFFF) {
                unsigned v = cp - 0x10000;
                snprintf(u, sizeof u, "\\u%04x\\u%04x", 0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
            } else {
                snprintf(u, sizeof u, "\\u%04x", cp);
            }
            w3_bputs(out, u);
            p += adv;
        }
    }
    w3_bputc(out, '"');
}

static void w3_dumps_rec(const json_t *v, w3_buf *out) {
    if (!v || v->type == JSON_NULL) { w3_bputs(out, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: w3_bputs(out, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char *s = w3_py_str(v);
            w3_bputs(out, s ? s : "0");
            free(s);
            break;
        }
        case JSON_STRING: w3_dumps_str(v->str_val, out); break;
        case JSON_ARRAY:
            w3_bputc(out, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) w3_bputs(out, ", ");
                w3_dumps_rec(v->c.items[i], out);
            }
            w3_bputc(out, ']');
            break;
        case JSON_OBJECT:
            w3_bputc(out, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) w3_bputs(out, ", ");
                w3_dumps_str(v->c.keys[i], out);
                w3_bputs(out, ": ");
                w3_dumps_rec(v->c.items[i], out);
            }
            w3_bputc(out, '}');
            break;
        default: w3_bputs(out, "null"); break;
    }
}

static char *w3_dumps_default(const json_t *v) {
    w3_buf out = {0};
    w3_dumps_rec(v, &out);
    return w3_bdone(&out);
}

/* Codepoint-aware truncation: first `limit` codepoints of a UTF-8 string. */
static char *w3_utf8_truncate(const char *s, size_t limit) {
    size_t n = strlen(s), off = 0, count = 0;
    while (off < n && count < limit) {
        size_t adv;
        w3_cp(s + off, &adv);
        off += adv;
        count++;
    }
    char *r = malloc(off + 1);
    if (!r) return NULL;
    memcpy(r, s, off);
    r[off] = '\0';
    return r;
}

static size_t w3_utf8_len(const char *s) {
    size_t n = strlen(s), off = 0, count = 0;
    while (off < n) {
        size_t adv;
        w3_cp(s + off, &adv);
        off += adv;
        count++;
    }
    return count;
}

/* _env_value(name): seam get_env_value; `result or ""`; except Exception
 * swallows plain raises (BaseException passes). Returns a caller-owned
 * node (any type — the CALLER applies .strip() where the live code does). */
static json_t *w3_env_value(const char *name) {
    json_t *args = json_object();
    json_set_hdr(args, "name", name);
    json_t *r = w3_seam("get_env_value", args);
    if (w3_is_raise(r)) {
        if (w3_is_base_exc(r)) return r;
        json_free(r);
        return json_string("");
    }
    if (!w3_truthy(r)) {
        json_free(r);
        return json_string("");
    }
    return r;
}

/* PoP: ws_u_normalize_whatsapp_allowed_users @ hermes_cli/web_routers/messaging.py:_normalize_whatsapp_allowed_users */
json_t *ws_u_normalize_whatsapp_allowed_users(json_t *req) {
    const json_t *value = json_obj_get(req, "value");
    char *s0 = w3_py_str_or_empty(value);
    char *raw = w3_strip(s0);
    free(s0);
    if (!raw || !raw[0]) {
        free(raw);
        return json_string("");
    }
    w3_buf out = {0};
    int emitted = 0;
    const char *p = raw;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t plen = comma ? (size_t)(comma - p) : strlen(p);
        char *part = malloc(plen + 1);
        if (!part) break;
        memcpy(part, p, plen);
        part[plen] = '\0';
        char *trimmed = w3_strip(part);
        if (trimmed && trimmed[0]) {
            if (emitted) w3_bputc(&out, ',');
            for (const char *q = part; *q; q++)
                if (*q != ' ') w3_bputc(&out, *q);
            emitted = 1;
        }
        free(trimmed);
        free(part);
        if (!comma) break;
        p = comma + 1;
    }
    free(raw);
    char *r = w3_bdone(&out);
    json_t *ret = json_string(r ? r : "");
    free(r);
    return ret;
}

/* PoP: ws_u_whatsapp_phone_from_identifier @ hermes_cli/web_routers/messaging.py:_whatsapp_phone_from_identifier */
json_t *ws_u_whatsapp_phone_from_identifier(json_t *req) {
    const json_t *value = json_obj_get(req, "value");
    char *s0 = w3_py_str_or_empty(value);
    char *raw = w3_strip(s0);
    free(s0);
    if (!raw || !raw[0]) {
        free(raw);
        return json_null();
    }
    /* raw.split("@",1)[0].split(":",1)[0], then re.sub(r"\D+","") —
     * ASCII digit scope (documented in the header). */
    w3_buf digits = {0};
    for (const char *q = raw; *q && *q != '@' && *q != ':'; q++)
        if (*q >= '0' && *q <= '9') w3_bputc(&digits, *q);
    free(raw);
    char *d = w3_bdone(&digits);
    json_t *ret = (d && d[0]) ? json_string(d) : json_null();
    free(d);
    return ret;
}

/* PoP: ws_u_normalize_whatsapp_onboarding_mode @ hermes_cli/web_routers/messaging.py:_normalize_whatsapp_onboarding_mode */
json_t *ws_u_normalize_whatsapp_onboarding_mode(json_t *req) {
    const json_t *value = json_obj_get(req, "value");
    char *s0 = w3_truthy(value) ? w3_py_str(value) : w3_strdup("bot");
    char *s1 = w3_strip(s0);
    free(s0);
    char *mode = w3_lower_ascii(s1);
    free(s1);
    if (!mode || (strcmp(mode, "bot") != 0 && strcmp(mode, "self-chat") != 0)) {
        free(mode);
        return w3_http_exc(400, "WhatsApp mode must be 'bot' or 'self-chat'.");
    }
    json_t *ret = json_string(mode);
    free(mode);
    return ret;
}

/* PoP: ws_u_string_list @ hermes_cli/web_server_memory.py:_string_list */
json_t *ws_u_string_list(json_t *req) {
    const json_t *value = json_obj_get(req, "value");
    json_t *out = json_array();
    if (!value || value->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < value->c.count; i++) {
        char *s = w3_py_str(value->c.items[i]);
        char *t = w3_strip(s);
        free(s);
        if (t && t[0]) json_append(out, json_string(t));
        free(t);
    }
    return out;
}

/* PoP: ws_u_resolve_toolset_model_plugin @ hermes_cli/web_routers/tools.py:_resolve_toolset_model_plugin */
json_t *ws_u_resolve_toolset_model_plugin(json_t *req) {
    const char *ts_key = json_get_str(req, "ts_key", "");
    const json_t *row = json_obj_get(req, "provider_row");
    if (!row || row->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", w3_typename(row));
        return w3_raise("AttributeError", msg);
    }
    if (strcmp(ts_key, "image_gen") == 0) {
        const json_t *a = json_obj_get(row, "image_gen_plugin_name");
        if (w3_truthy(a)) return json_copy(a);
        return w3_copy_or_null(json_obj_get(row, "imagegen_backend"));
    }
    if (strcmp(ts_key, "video_gen") == 0)
        return w3_copy_or_null(json_obj_get(row, "video_gen_plugin_name"));
    return json_null();
}

/* PoP: ws_u_chat_image_extension @ hermes_cli/web_routers/files.py:_chat_image_extension */
json_t *ws_u_chat_image_extension(json_t *req) {
    const char *hex = json_get_str(req, "data_hex", "");
    size_t hn = strlen(hex), digits = 0;
    for (size_t i = 0; i < hn; i++) {
        unsigned char c = (unsigned char)hex[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        int ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                 (c >= 'A' && c <= 'F');
        if (!ok) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "non-hexadecimal number found in fromhex() arg at position %zu", i);
            return w3_raise("ValueError", msg);
        }
        digits++;
    }
    if (digits % 2)
        return w3_raise("ValueError",
                        "fromhex() arg must contain an even number of hexadecimal digits");
    size_t n = digits / 2;
    unsigned char *data = malloc(n ? n : 1);
    if (!data) return json_null();
    size_t src = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char b = 0;
        for (int k = 0; k < 2; k++) {
            unsigned char c = 0;
            while (src < hn) {
                unsigned char h = (unsigned char)hex[src++];
                if (h == ' ' || h == '\t' || h == '\n' || h == '\r') continue;
                c = h;
                break;
            }
            unsigned v = (c >= '0' && c <= '9') ? (unsigned)(c - '0') :
                         (c >= 'a' && c <= 'f') ? (unsigned)(c - 'a' + 10) :
                         (c >= 'A' && c <= 'F') ? (unsigned)(c - 'A' + 10) : 0;
            b = (unsigned char)((b << 4) | v);
        }
        data[i] = b;
    }
    size_t head = n < 16 ? n : 16;
    const char *ext = NULL;
    if (head >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0) {
        ext = ".webp";
    } else {
        static const struct { const char *sig; size_t slen; const char *ext; } magic[] = {
            {"\x89PNG\r\n\x1a\n", 8, ".png"},
            {"\xff\xd8\xff", 3, ".jpg"},
            {"GIF87a", 6, ".gif"},
            {"GIF89a", 6, ".gif"},
            {"BM", 2, ".bmp"},
        };
        for (size_t i = 0; i < sizeof magic / sizeof magic[0]; i++) {
            if (head >= magic[i].slen && memcmp(data, magic[i].sig, magic[i].slen) == 0) {
                ext = magic[i].ext;
                break;
            }
        }
    }
    free(data);
    return ext ? json_string(ext) : json_null();
}

/* PoP: ws_u_api_key_display @ hermes_cli/web_routers/config_env.py:_api_key_display */
json_t *ws_u_api_key_display(json_t *req) {
    const json_t *entry = json_obj_get(req, "entry");
    char *p0 = w3_py_str_or_empty(json_obj_get(entry, "api_key"));
    char *plaintext = w3_strip(p0);
    free(p0);
    if (plaintext && plaintext[0]) {
        json_t *args = json_object();
        json_set_hdr(args, "value", plaintext);
        free(plaintext);
        json_t *preview = w3_seam("redacted_credential_preview", args);
        if (w3_is_raise(preview)) return preview;
        json_t *ret = json_array();
        json_append(ret, json_bool(true));
        json_append(ret, w3_copy_or_null(preview));
        json_free(preview);
        return ret;
    }
    free(plaintext);
    char *k0 = w3_py_str_or_empty(json_obj_get(entry, "key_env"));
    char *key_env = w3_strip(k0);
    free(k0);
    json_t *ret = json_array();
    if (key_env && key_env[0]) {
        json_append(ret, json_bool(true));
        w3_buf b = {0};
        w3_bputs(&b, "${");
        w3_bputs(&b, key_env);
        w3_bputc(&b, '}');
        char *s = w3_bdone(&b);
        json_append(ret, json_string(s ? s : ""));
        free(s);
    } else {
        json_append(ret, json_bool(false));
        json_append(ret, json_null());
    }
    free(key_env);
    return ret;
}

/* PoP: ws_u_oauth_provider_disconnect_hint @ hermes_cli/web_routers/oauth.py:_oauth_provider_disconnect_hint */
json_t *ws_u_oauth_provider_disconnect_hint(json_t *req) {
    const json_t *provider = json_obj_get(req, "provider");
    const json_t *status = json_obj_get(req, "status");
    const json_t *flow = json_obj_get(provider, "flow");
    const json_t *id = json_obj_get(provider, "id");
    int external = flow && flow->type == JSON_STRING && strcmp(flow->str_val, "external") == 0;
    int anthropic = id && id->type == JSON_STRING && strcmp(id->str_val, "anthropic") == 0;
    if (external && !anthropic) {
        /* _oauth_provider_disconnect_command(provider) with platform=None:
         * non-None only for claude-code; _claude_code_disconnect_command
         * runs with sys.platform pinned to "win32". */
        int claude_code = id && id->type == JSON_STRING && strcmp(id->str_val, "claude-code") == 0;
        if (claude_code)
            return json_string("Managed outside Hermes \xe2\x80\x94 run the disconnect command to remove it.");
        return json_string("Managed by that provider's CLI; remove it there.");
    }
    const json_t *source = json_obj_get(status, "source");
    if (source && source->type == JSON_STRING && strcmp(source->str_val, "env_var") == 0)
        return json_string("Remove the API key from Settings \xe2\x86\x92 Keys instead.");
    return json_null();
}
/* _http_response_error_detail(resp) — real sibling port (helper, no PoP
 * claim): resp.json() is the "resp.json" seam; except Exception swallows
 * plain raises, BaseException passes. */
static json_t *w3_http_error_detail(const json_t *resp) {
    json_t *payload = w3_seam("resp.json", json_object());
    if (w3_is_raise(payload)) {
        if (w3_is_base_exc(payload)) return payload;
        json_free(payload);
        payload = NULL;
    }
    if (payload && payload->type == JSON_OBJECT) {
        const json_t *error = json_obj_get(payload, "error");
        if (error && error->type == JSON_OBJECT) {
            static const char *keys[] = {"message", "error_description", "code", "type"};
            w3_buf parts = {0};
            int nparts = 0;
            for (int i = 0; i < 4; i++) {
                const json_t *v = json_obj_get(error, keys[i]);
                char *s = v ? w3_py_str(v) : w3_strdup("");
                char *t = w3_strip(s);
                free(s);
                if (t && t[0]) {
                    if (nparts) w3_bputs(&parts, ": ");
                    w3_bputs(&parts, t);
                    nparts++;
                }
                free(t);
            }
            if (nparts) {
                json_free(payload);
                char *joined = w3_bdone(&parts);
                json_t *ret = json_string(joined ? joined : "");
                free(joined);
                return ret;
            }
            free(w3_bdone(&parts));
        }
        const json_t *cands[4];
        cands[0] = error;
        cands[1] = json_obj_get(payload, "detail");
        cands[2] = json_obj_get(payload, "message");
        cands[3] = json_obj_get(payload, "error_description");
        for (int i = 0; i < 4; i++) {
            const json_t *v = cands[i];
            if (v && v->type == JSON_STRING) {
                char *t = w3_strip(v->str_val);
                if (t && t[0]) {
                    json_free(payload);
                    json_t *ret = json_string(t);
                    free(t);
                    return ret;
                }
                free(t);
            }
        }
    }
    if (payload) json_free(payload);
    char *s0 = w3_py_str_or_empty(json_obj_get(resp, "text"));
    char *s1 = w3_strip(s0);
    free(s0);
    char *cut = w3_utf8_truncate(s1 ? s1 : "", 500);
    free(s1);
    json_t *ret = json_string(cut ? cut : "");
    free(cut);
    return ret;
}

/* PoP: ws_u_codex_device_code_start_error @ hermes_cli/web_routers/oauth.py:_codex_device_code_start_error */
json_t *ws_u_codex_device_code_start_error(json_t *req) {
    const json_t *resp = json_obj_get(req, "resp");
    char *status;
    const json_t *sc = json_obj_get(resp, "status_code");
    if (sc) status = w3_py_str(sc);
    else status = w3_strdup("unknown");
    json_t *detail_node = w3_http_error_detail(resp);
    if (w3_is_raise(detail_node)) {
        free(status);
        return detail_node;
    }
    const char *detail = detail_node->str_val ? detail_node->str_val : "";
    char *lower = w3_lower_ascii(detail);
    int device_branch = strstr(lower, "device") != NULL &&
                        (strstr(lower, "authori") != NULL || strstr(lower, "enable") != NULL);
    free(lower);
    const char *message = device_branch
        ? "OpenAI rejected the device-code login request. Your OpenAI "
          "account may need device-code authorization enabled before Hermes "
          "can start this dashboard login. Enable device-code authorization "
          "in OpenAI, then return here and click Login again."
        : "OpenAI rejected the device-code login request. Please try Login "
          "again from the dashboard after checking your OpenAI account settings.";
    w3_buf out = {0};
    w3_bputs(&out, message);
    w3_bputs(&out, " (HTTP ");
    w3_bputs(&out, status ? status : "unknown");
    if (detail[0]) {
        w3_bputs(&out, ": ");
        w3_bputs(&out, detail);
    }
    w3_bputc(&out, ')');
    free(status);
    json_free(detail_node);
    char *s = w3_bdone(&out);
    json_t *ret = json_string(s ? s : "");
    free(s);
    return ret;
}

/* PoP: ws_u_read_field @ hermes_cli/web_routers/memory_providers.py:_read_field */
json_t *ws_u_read_field(json_t *req) {
    const json_t *field = json_obj_get(req, "field");
    const json_t *sources = json_obj_get(req, "sources");
    const json_t *env = json_obj_get(req, "env");
    const char *key = json_get_str(field, "key", "");
    const json_t *aliases = json_obj_get(field, "aliases");
    const json_t *fallbacks = json_obj_get(field, "env_fallbacks");
    size_t nsources = (sources && sources->type == JSON_ARRAY) ? sources->c.count : 0;
    for (size_t si = 0; si < nsources; si++) {
        const json_t *source = sources->c.items[si];
        if (!source || source->type != JSON_OBJECT) {
            char msg[128];
            snprintf(msg, sizeof msg, "argument of type '%s' is not iterable", w3_typename(source));
            return w3_raise("TypeError", msg);
        }
        /* (field.key, *field.aliases) — presence decides, not truthiness. */
        const char *keys[64];
        size_t nk = 1;
        keys[0] = key;
        if (aliases && aliases->type == JSON_ARRAY) {
            for (size_t i = 0; i < aliases->c.count && nk < 64; i++) {
                const json_t *a = aliases->c.items[i];
                keys[nk++] = (a && a->type == JSON_STRING) ? a->str_val : "";
            }
        }
        for (size_t ki = 0; ki < nk; ki++) {
            if (!json_has(source, keys[ki])) continue;
            const json_t *v = json_obj_get(source, keys[ki]);
            if (v && v->type != JSON_NULL) return json_copy(v);
        }
    }
    if (fallbacks && fallbacks->type == JSON_ARRAY) {
        for (size_t i = 0; i < fallbacks->c.count; i++) {
            const json_t *ek = fallbacks->c.items[i];
            if (!ek || ek->type != JSON_STRING) continue;
            const json_t *v = json_obj_get(env, ek->str_val);
            if (w3_truthy(v)) return json_copy(v);
        }
    }
    return json_null();
}

/* PoP: ws_u_declared_field_is_set @ hermes_cli/web_routers/memory_providers.py:_declared_field_is_set */
json_t *ws_u_declared_field_is_set(json_t *req) {
    const json_t *field = json_obj_get(req, "field");
    const json_t *sources = json_obj_get(req, "sources");
    const json_t *env = json_obj_get(req, "env");
    const json_t *env_key = json_obj_get(field, "env_key");
    const json_t *fallbacks = json_obj_get(field, "env_fallbacks");
    /* any(env_key and env.get(env_key) for env_key in (field.env_key, *field.env_fallbacks)) */
    const json_t *env_keys[65];
    size_t nek = 0;
    env_keys[nek++] = env_key;
    if (fallbacks && fallbacks->type == JSON_ARRAY)
        for (size_t i = 0; i < fallbacks->c.count && nek < 65; i++)
            env_keys[nek++] = fallbacks->c.items[i];
    for (size_t i = 0; i < nek; i++) {
        const json_t *ek = env_keys[i];
        if (!w3_truthy(ek)) continue;
        if (ek->type != JSON_STRING) continue;  /* non-str key: no match in a str-keyed env */
        if (w3_truthy(json_obj_get(env, ek->str_val))) return json_bool(true);
    }
    const char *key = json_get_str(field, "key", "");
    const json_t *aliases = json_obj_get(field, "aliases");
    const char *keys[64];
    size_t nk = 1;
    keys[0] = key;
    if (aliases && aliases->type == JSON_ARRAY)
        for (size_t i = 0; i < aliases->c.count && nk < 64; i++) {
            const json_t *a = aliases->c.items[i];
            keys[nk++] = (a && a->type == JSON_STRING) ? a->str_val : "";
        }
    size_t nsources = (sources && sources->type == JSON_ARRAY) ? sources->c.count : 0;
    for (size_t si = 0; si < nsources; si++) {
        const json_t *source = sources->c.items[si];
        for (size_t ki = 0; ki < nk; ki++)
            if (w3_truthy(json_obj_get(source, keys[ki]))) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: ws_u_serialize_field_value @ hermes_cli/web_routers/memory_providers.py:_serialize_field_value */
json_t *ws_u_serialize_field_value(json_t *req) {
    const json_t *field = json_obj_get(req, "field");
    const json_t *value = json_obj_get(req, "value");
    const char *kind = json_get_str(field, "kind", "");
    if (!value || value->type == JSON_NULL)
        return json_string(json_get_str(field, "default", ""));
    if (strcmp(kind, "bool") == 0) {
        /* utils.is_truthy_value(value): None -> default False; str ->
         * strip().lower() in TRUTHY_STRINGS; else bool(value). */
        int truthy;
        if (value->type == JSON_STRING) {
            char *s1 = w3_strip(value->str_val);
            char *s2 = w3_lower_ascii(s1);
            free(s1);
            truthy = s2 && (strcmp(s2, "1") == 0 || strcmp(s2, "true") == 0 ||
                            strcmp(s2, "yes") == 0 || strcmp(s2, "on") == 0);
            free(s2);
        } else {
            truthy = w3_truthy(value);
        }
        return json_string(truthy ? "true" : "false");
    }
    if (strcmp(kind, "json") == 0 &&
        (value->type == JSON_OBJECT || value->type == JSON_ARRAY)) {
        char *s = w3_dumps_default(value);
        json_t *ret = json_string(s ? s : "");
        free(s);
        return ret;
    }
    char *s = w3_py_str(value);
    json_t *ret = json_string(s ? s : "");
    free(s);
    return ret;
}

/* _trim_setup_output(value, limit=4000) — real sibling port (helper). */
static char *w3_trim_setup_output(const json_t *value) {
    char *text = w3_py_str_or_empty(value);
    if (!text) return w3_strdup("");
    if (w3_utf8_len(text) <= 4000) return text;
    char *cut = w3_utf8_truncate(text, 4000);
    free(text);
    w3_buf b = {0};
    w3_bputs(&b, cut ? cut : "");
    free(cut);
    w3_bputs(&b, "\n... truncated ...");
    return w3_bdone(&b);
}

/* PoP: ws_u_command_result @ hermes_cli/web_routers/memory_providers.py:_command_result */
json_t *ws_u_command_result(json_t *req) {
    const json_t *completed = json_obj_get(req, "completed");
    int have_completed = completed && completed->type != JSON_NULL;
    const json_t *error = json_obj_get(req, "error");
    json_t *out = json_object();
    json_set_hdr(out, "kind", json_get_str(req, "kind", ""));
    json_set_hdr(out, "name", json_get_str(req, "name", ""));
    json_set_hdr(out, "status", json_get_str(req, "status", ""));
    json_set_hdr(out, "command", json_get_str(req, "command", ""));
    json_set(out, "returncode",
             have_completed ? w3_copy_or_null(json_obj_get(completed, "returncode")) : json_null());
    if (have_completed) {
        char *t = w3_trim_setup_output(json_obj_get(completed, "stdout"));
        json_set_hdr(out, "stdout", t ? t : "");
        free(t);
    } else {
        json_set_hdr(out, "stdout", "");
    }
    /* stderr = _trim_setup_output(error or ("" if completed is None else completed.stderr)) */
    const json_t *stderr_src;
    if (w3_truthy(error)) stderr_src = error;
    else if (have_completed) stderr_src = json_obj_get(completed, "stderr");
    else stderr_src = NULL;
    char *t = w3_trim_setup_output(stderr_src);
    json_set_hdr(out, "stderr", t ? t : "");
    free(t);
    return out;
}

/* PoP: ws_u_terminal_cfg_value @ hermes_cli/web_routers/tools.py:_terminal_cfg_value */
json_t *ws_u_terminal_cfg_value(json_t *req) {
    const json_t *cfg = json_obj_get(req, "terminal_cfg");
    const char *key = json_get_str(req, "key", "");
    const char *env_var = json_get_str(req, "env_var", "");
    const json_t *value = json_obj_get(cfg, key);
    if (value && value->type != JSON_NULL) {
        char *s = w3_py_str(value);
        char *t = w3_strip(s);
        free(s);
        if (t && t[0]) {
            json_t *ret = json_string(t);
            free(t);
            return ret;
        }
        free(t);
    }
    json_t *ev = w3_env_value(env_var);
    if (w3_is_raise(ev)) return ev;
    if (ev->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", w3_typename(ev));
        json_free(ev);
        return w3_raise("AttributeError", msg);
    }
    char *t = w3_strip(ev->str_val);
    json_free(ev);
    json_t *ret = json_string(t ? t : "");
    free(t);
    return ret;
}

static json_t *w3_call_terminal_cfg(const json_t *cfg, const char *key, const char *env_var) {
    json_t *sub = json_object();
    json_set(sub, "terminal_cfg", w3_copy_or_null(cfg));
    json_set_hdr(sub, "key", key);
    json_set_hdr(sub, "env_var", env_var);
    json_t *r = ws_u_terminal_cfg_value(sub);
    json_free(sub);
    return r;
}

/* PoP: ws_u_probe_ssh_backend @ hermes_cli/web_routers/tools.py:_probe_ssh_backend */
json_t *ws_u_probe_ssh_backend(json_t *req) {
    const json_t *cfg = json_obj_get(req, "terminal_cfg");
    json_t *host = w3_call_terminal_cfg(cfg, "ssh_host", "TERMINAL_SSH_HOST");
    if (w3_is_raise(host)) return host;
    json_t *user = w3_call_terminal_cfg(cfg, "ssh_user", "TERMINAL_SSH_USER");
    if (w3_is_raise(user)) {
        json_free(host);
        return user;
    }
    const char *hs = host->str_val ? host->str_val : "";
    const char *us = user->str_val ? user->str_val : "";
    const char *missing[2];
    size_t nm = 0;
    if (!hs[0]) missing[nm++] = "terminal.ssh_host";
    if (!us[0]) missing[nm++] = "terminal.ssh_user";
    json_t *ret = json_array();
    if (nm) {
        w3_buf b = {0};
        w3_bputs(&b, "Set ");
        for (size_t i = 0; i < nm; i++) {
            if (i) w3_bputs(&b, " and ");
            w3_bputs(&b, missing[i]);
        }
        w3_bputs(&b, " in config.yaml (or the matching TERMINAL_SSH_* env vars).");
        char *s = w3_bdone(&b);
        json_append(ret, json_string("needs_setup"));
        json_append(ret, json_string(s ? s : ""));
        free(s);
    } else {
        w3_buf b = {0};
        w3_bputs(&b, us);
        w3_bputc(&b, '@');
        w3_bputs(&b, hs);
        char *s = w3_bdone(&b);
        json_append(ret, json_string("ready"));
        json_append(ret, json_string(s ? s : ""));
        free(s);
    }
    json_free(host);
    json_free(user);
    return ret;
}

/* PoP: ws_u_probe_modal_backend @ hermes_cli/web_routers/tools.py:_probe_modal_backend */
json_t *ws_u_probe_modal_backend(json_t *req) {
    (void)req;
    json_t *r = w3_seam("has_direct_modal_credentials", json_object());
    if (w3_is_raise(r)) {
        if (w3_is_base_exc(r)) return r;
        json_free(r);
    } else if (w3_truthy(r)) {
        json_free(r);
        json_t *ret = json_array();
        json_append(ret, json_string("ready"));
        json_append(ret, json_string(""));
        return ret;
    } else {
        json_free(r);
    }
    json_t *e1 = w3_env_value("MODAL_TOKEN_ID");
    if (w3_is_raise(e1)) return e1;
    if (w3_truthy(e1)) {
        json_t *e2 = w3_env_value("MODAL_TOKEN_SECRET");
        if (w3_is_raise(e2)) {
            json_free(e1);
            return e2;
        }
        if (w3_truthy(e2)) {
            json_free(e1);
            json_free(e2);
            json_t *ret = json_array();
            json_append(ret, json_string("ready"));
            json_append(ret, json_string(""));
            return ret;
        }
        json_free(e2);
    }
    json_free(e1);
    json_t *ret = json_array();
    json_append(ret, json_string("needs_setup"));
    json_append(ret, json_string("Modal credentials not found \xe2\x80\x94 set MODAL_TOKEN_ID and MODAL_TOKEN_SECRET (or run `modal setup`)."));
    return ret;
}
/* PoP: ws_u_find_toolset_provider_row @ hermes_cli/web_routers/tools.py:_find_toolset_provider_row */
json_t *ws_u_find_toolset_provider_row(json_t *req) {
    const char *ts_key = json_get_str(req, "ts_key", "");
    const json_t *config = json_obj_get(req, "config");
    const json_t *provider = json_obj_get(req, "provider");
    json_t *args = json_object();
    json_set_hdr(args, "ts_key", ts_key);
    json_set(args, "config", w3_copy_or_null(config));
    json_t *rows = w3_seam("_category_providers", args);
    if (w3_is_raise(rows)) return rows;
    if (!rows || rows->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", w3_typename(rows));
        json_free(rows);
        return w3_raise("TypeError", msg);
    }
    json_t *ret = json_null();
    if (w3_truthy(provider)) {
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *p = rows->c.items[i];
            const json_t *name = json_obj_get(p, "name");
            if (w3_json_eq(name, provider)) {
                ret = json_copy(p);
                break;
            }
        }
    } else {
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *p = rows->c.items[i];
            json_t *args2 = json_object();
            json_set(args2, "p", json_copy(p));
            json_set(args2, "config", w3_copy_or_null(config));
            json_set(args2, "force_fresh", json_bool(true));
            json_t *act = w3_seam("_is_provider_active", args2);
            if (w3_is_raise(act)) {
                json_free(rows);
                return act;
            }
            int active = w3_truthy(act);
            json_free(act);
            if (active) {
                ret = json_copy(p);
                break;
            }
        }
    }
    json_free(rows);
    return ret;
}

/* Collect strdup'd keys of stale entries (two-pass: the live list
 * comprehension is fully evaluated BEFORE any pop). */
typedef struct { const char **keys; size_t count, cap; } w3_keylist;

static void w3_keylist_add(w3_keylist *kl, const char *key) {
    if (kl->count == kl->cap) {
        size_t nc = kl->cap ? kl->cap * 2 : 16;
        const char **nk = realloc(kl->keys, nc * sizeof *nk);
        if (!nk) return;
        kl->keys = nk;
        kl->cap = nc;
    }
    kl->keys[kl->count++] = w3_strdup(key);
}

static void w3_keylist_free(w3_keylist *kl) {
    for (size_t i = 0; i < kl->count; i++) free((void *)kl->keys[i]);
    free(kl->keys);
    kl->keys = NULL;
    kl->count = kl->cap = 0;
}

static json_t *w3_time_seam(void) {
    return w3_seam("time.time", json_object());
}

/* PoP: ws_u_gc_oauth_sessions @ hermes_cli/web_routers/oauth.py:_gc_oauth_sessions */
json_t *ws_u_gc_oauth_sessions(json_t *req) {
    json_t *sessions = json_obj_get(req, "oauth_sessions");
    if (!sessions || sessions->type != JSON_OBJECT) {
        sessions = json_object();
        json_set(req, "oauth_sessions", sessions);
    }
    json_t *now = w3_time_seam();
    if (w3_is_raise(now)) return now;
    if (now->type != JSON_NUMBER) {
        char msg[160];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for -: '%s' and 'int'", w3_typename(now));
        json_free(now);
        return w3_raise("TypeError", msg);
    }
    double cutoff = now->num_val - 900.0;  /* _OAUTH_SESSION_TTL_SECONDS = 15 * 60 */
    const char *cutoff_type = cutoff == (double)(long long)cutoff ? "int" : "float";
    json_free(now);
    w3_keylist stale = {0};
    for (size_t i = 0; i < sessions->c.count; i++) {
        const json_t *sess = sessions->c.items[i];
        const json_t *created = json_obj_get(sess, "created_at");
        if (!created) {
            w3_keylist_free(&stale);
            return w3_raise("KeyError", "'created_at'");
        }
        if (created->type != JSON_NUMBER) {
            char msg[160];
            snprintf(msg, sizeof msg, "'<' not supported between instances of '%s' and '%s'",
                     w3_typename(created), cutoff_type);
            w3_keylist_free(&stale);
            return w3_raise("TypeError", msg);
        }
        if (created->num_val < cutoff) w3_keylist_add(&stale, sessions->c.keys[i]);
    }
    for (size_t i = 0; i < stale.count; i++) json_obj_del(sessions, stale.keys[i]);
    w3_keylist_free(&stale);
    return json_null();
}

/* PoP: ws_u_gc_mcp_oauth_flows @ hermes_cli/web_routers/mcp.py:_gc_mcp_oauth_flows */
json_t *ws_u_gc_mcp_oauth_flows(json_t *req) {
    json_t *flows = json_obj_get(req, "mcp_oauth_flows");
    if (!flows || flows->type != JSON_OBJECT) {
        flows = json_object();
        json_set(req, "mcp_oauth_flows", flows);
    }
    json_t *now = w3_time_seam();
    if (w3_is_raise(now)) return now;
    if (now->type != JSON_NUMBER) {
        char msg[160];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for -: '%s' and 'int'", w3_typename(now));
        json_free(now);
        return w3_raise("TypeError", msg);
    }
    double cutoff = now->num_val - 900.0;  /* _MCP_DASHBOARD_OAUTH_TTL = 15 * 60 */
    const char *cutoff_type = cutoff == (double)(long long)cutoff ? "int" : "float";
    json_free(now);
    w3_keylist stale = {0};
    for (size_t i = 0; i < flows->c.count; i++) {
        const json_t *flow = flows->c.items[i];
        /* getattr(flow, "created_at", 0) — absent reads as 0 */
        const json_t *created = json_obj_get(flow, "created_at");
        double cval = 0.0;
        if (created) {
            if (created->type != JSON_NUMBER) {
                char msg[160];
                snprintf(msg, sizeof msg, "'<' not supported between instances of '%s' and '%s'",
                         w3_typename(created), cutoff_type);
                w3_keylist_free(&stale);
                return w3_raise("TypeError", msg);
            }
            cval = created->num_val;
        }
        if (cval < cutoff) w3_keylist_add(&stale, flows->c.keys[i]);
    }
    for (size_t i = 0; i < stale.count; i++) json_obj_del(flows, stale.keys[i]);
    w3_keylist_free(&stale);
    return json_null();
}

/* PoP: ws_u_prune_telegram_onboarding_pairings @ hermes_cli/web_routers/messaging.py:_prune_telegram_onboarding_pairings */
json_t *ws_u_prune_telegram_onboarding_pairings(json_t *req) {
    json_t *pairings = json_obj_get(req, "telegram_onboarding_pairings");
    if (!pairings || pairings->type != JSON_OBJECT) {
        pairings = json_object();
        json_set(req, "telegram_onboarding_pairings", pairings);
    }
    json_t *now = w3_time_seam();
    if (w3_is_raise(now)) return now;
    /* now is only compared (no arithmetic): a non-number raises at the
     * FIRST <= comparison, and an empty store never compares at all. */
    double nowval = now->type == JSON_NUMBER ? now->num_val : 0.0;
    const char *now_type = w3_typename(now);
    w3_keylist stale = {0};
    for (size_t i = 0; i < pairings->c.count; i++) {
        const json_t *record = pairings->c.items[i];
        const json_t *expires = json_obj_get(record, "expires_at_ts");
        if (!expires) {
            w3_keylist_free(&stale);
            json_free(now);
            return w3_raise("AttributeError",
                            "'types.SimpleNamespace' object has no attribute 'expires_at_ts'");
        }
        if (expires->type != JSON_NUMBER || now->type != JSON_NUMBER) {
            char msg[160];
            snprintf(msg, sizeof msg, "'<=' not supported between instances of '%s' and '%s'",
                     w3_typename(expires), now_type);
            w3_keylist_free(&stale);
            json_free(now);
            return w3_raise("TypeError", msg);
        }
        if (expires->num_val <= nowval) w3_keylist_add(&stale, pairings->c.keys[i]);
    }
    json_free(now);
    for (size_t i = 0; i < stale.count; i++) json_obj_del(pairings, stale.keys[i]);
    w3_keylist_free(&stale);
    return json_null();
}

/* re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")[:48] or fallback */
static char *w3_action_slug(const char *name, const char *fallback) {
    char *lower = w3_lower_ascii(name ? name : "");
    w3_buf b = {0};
    int in_run = 0;
    for (const char *p = lower; *p; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) {
            w3_bputc(&b, *p);
            in_run = 0;
        } else if (!in_run) {
            w3_bputc(&b, '-');
            in_run = 1;
        }
    }
    free(lower);
    char *s = w3_bdone(&b);
    const char *start = s;
    while (*start == '-') start++;
    const char *end = s + strlen(s);
    while (end > start && end[-1] == '-') end--;
    size_t len = (size_t)(end - start);
    if (len > 48) len = 48;
    char *r = malloc(len + 1);
    if (!r) { free(s); return w3_strdup(fallback); }
    memcpy(r, start, len);
    r[len] = '\0';
    free(s);
    if (!r[0]) {
        free(r);
        return w3_strdup(fallback);
    }
    return r;
}

static json_t *w3_action_log_files(json_t *req) {
    json_t *alf = json_obj_get(req, "action_log_files");
    if (!alf || alf->type != JSON_OBJECT) {
        alf = json_object();
        json_set(req, "action_log_files", alf);
    }
    return alf;
}

static void w3_register_action_log(json_t *alf, const char *action) {
    if (json_has(alf, action)) return;  /* dict.setdefault */
    w3_buf b = {0};
    w3_bputs(&b, "action-");
    w3_bputs(&b, action);
    w3_bputs(&b, ".log");
    char *s = w3_bdone(&b);
    json_set_hdr(alf, action, s ? s : "");
    free(s);
}

/* PoP: ws_u_mcp_install_action_name @ hermes_cli/web_routers/mcp.py:_mcp_install_action_name */
json_t *ws_u_mcp_install_action_name(json_t *req) {
    const char *name = json_get_str(req, "name", "");
    json_t *alf = w3_action_log_files(req);
    char *slug = w3_action_slug(name, "server");
    char *digest = w3_sha1_hex((const unsigned char *)name, strlen(name));
    char d8[9];
    memcpy(d8, digest ? digest : "00000000", 8);
    d8[8] = '\0';
    free(digest);
    w3_buf b = {0};
    w3_bputs(&b, "mcp-install-");
    w3_bputs(&b, slug);
    w3_bputc(&b, '-');
    w3_bputs(&b, d8);
    free(slug);
    char *action = w3_bdone(&b);
    w3_register_action_log(alf, action ? action : "");
    json_t *ret = json_string(action ? action : "");
    free(action);
    return ret;
}

/* PoP: ws_u_hub_action_name @ hermes_cli/web_server_profiles.py:_hub_action_name */
json_t *ws_u_hub_action_name(json_t *req) {
    const char *verb = json_get_str(req, "verb", "");
    const char *key = json_get_str(req, "key", "");
    const json_t *profile = json_obj_get(req, "profile");
    json_t *alf = w3_action_log_files(req);
    char *p0 = w3_truthy(profile) ? w3_py_str(profile) : w3_strdup("current");
    char *p1 = w3_strip(p0);
    free(p0);
    char *profile_key = w3_lower_ascii(p1);
    free(p1);
    char *slug = w3_action_slug(key, "skill");
    w3_buf db = {0};
    w3_bputs(&db, profile_key ? profile_key : "current");
    w3_bputc(&db, ':');
    w3_bputs(&db, key);
    char *dinput = w3_bdone(&db);
    free(profile_key);
    char *digest = w3_sha1_hex((const unsigned char *)dinput, strlen(dinput));
    free(dinput);
    char d8[9];
    memcpy(d8, digest ? digest : "00000000", 8);
    d8[8] = '\0';
    free(digest);
    w3_buf b = {0};
    w3_bputs(&b, "skills-");
    w3_bputs(&b, verb);
    w3_bputc(&b, '-');
    w3_bputs(&b, slug);
    w3_bputc(&b, '-');
    w3_bputs(&b, d8);
    free(slug);
    char *name = w3_bdone(&b);
    w3_register_action_log(alf, name ? name : "");
    json_t *ret = json_string(name ? name : "");
    free(name);
    return ret;
}

/* PoP: ws_u_profile_cli_args @ hermes_cli/web_server_profiles.py:_profile_cli_args */
json_t *ws_u_profile_cli_args(json_t *req) {
    const json_t *profile = json_obj_get(req, "profile");
    char *p0 = w3_py_str_or_empty(profile);
    char *requested = w3_strip(p0);
    free(p0);
    char *lower = w3_lower_ascii(requested);
    int skip = !requested[0] || strcmp(lower, "current") == 0;
    free(lower);
    if (skip) {
        free(requested);
        return json_array();
    }
    json_t *args = json_object();
    json_set_hdr(args, "name", requested);
    json_t *r = w3_seam("_resolve_profile_dir", args);
    if (w3_is_raise(r)) {
        free(requested);
        return r;
    }
    json_free(r);
    json_t *args2 = json_object();
    json_set_hdr(args2, "name", requested);
    free(requested);
    json_t *r2 = w3_seam("normalize_profile_name", args2);
    if (w3_is_raise(r2)) return r2;
    json_t *ret = json_array();
    json_append(ret, json_string("-p"));
    json_append(ret, w3_copy_or_null(r2));
    json_free(r2);
    return ret;
}

/* PoP: ws_u_pairing_store @ hermes_cli/web_routers/ops.py:_pairing_store */
json_t *ws_u_pairing_store(json_t *req) {
    const json_t *profile = json_obj_get(req, "profile");
    char *p0 = w3_py_str_or_empty(profile);
    char *requested = w3_strip(p0);
    free(p0);
    char *lower = w3_lower_ascii(requested);
    int current = !requested[0] || strcmp(lower, "current") == 0;
    free(lower);
    if (current) {
        free(requested);
        return w3_seam("PairingStore", json_object());
    }
    json_t *args = json_object();
    json_set_hdr(args, "name", requested);
    json_t *r = w3_seam("_resolve_profile_dir", args);
    if (w3_is_raise(r)) {
        free(requested);
        return r;
    }
    json_free(r);
    json_t *args2 = json_object();
    json_set_hdr(args2, "profile", requested);
    free(requested);
    return w3_seam("PairingStore", args2);
}

/* PoP: ws_u_memory_provider_options @ hermes_cli/web_server_config.py:_memory_provider_options */
json_t *ws_u_memory_provider_options(json_t *req) {
    (void)req;
    json_t *options = json_array();
    json_append(options, json_string(""));
    json_t *r = w3_seam("list_memory_provider_names", json_object());
    if (w3_is_raise(r)) {
        if (w3_is_base_exc(r)) {
            json_free(options);
            return r;
        }
        json_free(r);
    } else if (r) {
        /* options.extend(result): list -> items, str -> codepoints,
         * dict -> keys; anything else raises TypeError (swallowed). */
        if (r->type == JSON_ARRAY) {
            for (size_t i = 0; i < r->c.count; i++)
                json_append(options, json_copy(r->c.items[i]));
        } else if (r->type == JSON_STRING) {
            const char *p = r->str_val ? r->str_val : "";
            while (*p) {
                size_t adv;
                w3_cp(p, &adv);
                char *cp = malloc(adv + 1);
                if (cp) {
                    memcpy(cp, p, adv);
                    cp[adv] = '\0';
                    json_append(options, json_string(cp));
                    free(cp);
                }
                p += adv;
            }
        } else if (r->type == JSON_OBJECT) {
            for (size_t i = 0; i < r->c.count; i++)
                json_append(options, json_string(r->c.keys[i]));
        }
        json_free(r);
    }
    /* list(dict.fromkeys(options)) — order-preserving dedup */
    json_t *out = json_array();
    for (size_t i = 0; i < options->c.count; i++) {
        int dup = 0;
        for (size_t j = 0; j < out->c.count; j++) {
            if (w3_json_eq(options->c.items[i], out->c.items[j])) {
                dup = 1;
                break;
            }
        }
        if (!dup) json_append(out, json_copy(options->c.items[i]));
    }
    json_free(options);
    return out;
}

/* PoP: ws_u_validate_dashboard_cron_context_from @ hermes_cli/web_server_cron.py:_validate_dashboard_cron_context_from */
json_t *ws_u_validate_dashboard_cron_context_from(json_t *req) {
    const json_t *refs = json_obj_get(req, "refs");
    const json_t *pname = json_obj_get(req, "profile_name");
    char *pname_s = w3_py_str(pname);
    json_t *reflist = json_array();
    if (refs && refs->type == JSON_ARRAY) {
        for (size_t i = 0; i < refs->c.count; i++)
            json_append(reflist, json_copy(refs->c.items[i]));
    } else if (refs && refs->type == JSON_STRING) {
        const char *p = refs->str_val ? refs->str_val : "";
        while (*p) {
            size_t adv;
            w3_cp(p, &adv);
            char *cp = malloc(adv + 1);
            if (cp) {
                memcpy(cp, p, adv);
                cp[adv] = '\0';
                json_append(reflist, json_string(cp));
                free(cp);
            }
            p += adv;
        }
    } else if (refs && refs->type == JSON_OBJECT) {
        for (size_t i = 0; i < refs->c.count; i++)
            json_append(reflist, json_string(refs->c.keys[i]));
    } else if (refs && refs->type != JSON_NULL) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", w3_typename(refs));
        json_free(reflist);
        free(pname_s);
        return w3_raise("TypeError", msg);
    }
    for (size_t i = 0; i < reflist->c.count; i++) {
        const json_t *ref = reflist->c.items[i];
        if (ref && ref->type == JSON_STRING) {
            char *t = w3_strip(ref->str_val);
            char *l = w3_lower_ascii(t);
            free(t);
            int is_self = l && strcmp(l, "self") == 0;
            free(l);
            if (is_self) continue;
        }
        json_t *args = json_object();
        json_set(args, "profile_name", w3_copy_or_null(pname));
        json_set_hdr(args, "method", "get_job");
        json_set(args, "ref", json_copy(ref));
        json_t *r = w3_seam("_call_cron_for_profile", args);
        if (w3_is_raise(r)) {
            json_free(reflist);
            free(pname_s);
            return r;
        }
        if (!w3_truthy(r)) {
            char *rs = w3_py_str(ref);
            w3_buf b = {0};
            w3_bputs(&b, "context_from job '");
            w3_bputs(&b, rs ? rs : "None");
            w3_bputs(&b, "' not found in profile '");
            w3_bputs(&b, pname_s ? pname_s : "None");
            w3_bputc(&b, '\'');
            free(rs);
            char *detail = w3_bdone(&b);
            json_free(r);
            json_free(reflist);
            free(pname_s);
            json_t *exc = w3_http_exc(400, detail ? detail : "");
            free(detail);
            return exc;
        }
        json_free(r);
    }
    json_free(reflist);
    free(pname_s);
    return json_null();
}

static int w3_sort_cmp(const void *a, const void *b) {
    const json_t *x = *(const json_t *const *)a;
    const json_t *y = *(const json_t *const *)b;
    if (x->type != y->type) return (int)x->type - (int)y->type;
    switch (x->type) {
        case JSON_NUMBER:
            return (x->num_val < y->num_val) ? -1 : (x->num_val > y->num_val) ? 1 : 0;
        case JSON_STRING:
            return strcmp(x->str_val ? x->str_val : "", y->str_val ? y->str_val : "");
        case JSON_BOOL:
            return (int)x->bool_val - (int)y->bool_val;
        default:
            return 0;
    }
}

/* PoP: ws_u_channel_managed_env_keys @ hermes_cli/web_server_messaging.py:_channel_managed_env_keys */
json_t *ws_u_channel_managed_env_keys(json_t *req) {
    (void)req;
    json_t *r = w3_seam("_messaging_platform_catalog", json_object());
    if (w3_is_raise(r)) {
        if (w3_is_base_exc(r)) return r;
        json_free(r);
        return json_array();
    }
    json_t *keys = json_array();
    int failed = 0;
    /* frozenset(k for entry in catalog for k in entry.get("env_vars", ()))
     * — any AttributeError/TypeError/unhashable element aborts the whole
     * comprehension into except Exception -> frozenset(). */
    if (!r || r->type == JSON_NULL || r->type == JSON_NUMBER || r->type == JSON_BOOL) {
        failed = 1;
    } else if (r->type == JSON_STRING || r->type == JSON_OBJECT) {
        /* iterating yields str keys/chars; str.get raises AttributeError */
        size_t n = r->type == JSON_STRING ? w3_utf8_len(r->str_val ? r->str_val : "") : r->c.count;
        if (n > 0) failed = 1;
    } else if (r->type == JSON_ARRAY) {
        for (size_t i = 0; i < r->c.count && !failed; i++) {
            const json_t *entry = r->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) {
                failed = 1;
                break;
            }
            const json_t *ev = json_obj_get(entry, "env_vars");
            if (!ev) continue;  /* .get default () */
            const json_t *items = NULL;
            json_t *tmp = NULL;
            if (ev->type == JSON_ARRAY) {
                items = ev;
            } else if (ev->type == JSON_STRING) {
                tmp = json_array();
                const char *p = ev->str_val ? ev->str_val : "";
                while (*p) {
                    size_t adv;
                    w3_cp(p, &adv);
                    char *cp = malloc(adv + 1);
                    if (cp) {
                        memcpy(cp, p, adv);
                        cp[adv] = '\0';
                        json_append(tmp, json_string(cp));
                        free(cp);
                    }
                    p += adv;
                }
                items = tmp;
            } else if (ev->type == JSON_OBJECT) {
                tmp = json_array();
                for (size_t k = 0; k < ev->c.count; k++)
                    json_append(tmp, json_string(ev->c.keys[k]));
                items = tmp;
            } else {
                failed = 1;
                break;
            }
            for (size_t k = 0; k < items->c.count && !failed; k++) {
                const json_t *key = items->c.items[k];
                if (!key || key->type == JSON_ARRAY || key->type == JSON_OBJECT) {
                    failed = 1;  /* unhashable */
                    break;
                }
                int dup = 0;
                for (size_t j = 0; j < keys->c.count; j++) {
                    if (w3_json_eq(key, keys->c.items[j])) {
                        dup = 1;
                        break;
                    }
                }
                if (!dup) json_append(keys, json_copy(key));
            }
            if (tmp) json_free(tmp);
        }
    }
    json_free(r);
    if (failed) {
        json_free(keys);
        return json_array();
    }
    if (keys->c.count > 1)
        qsort(keys->c.items, keys->c.count, sizeof(json_t *), w3_sort_cmp);
    return keys;
}
