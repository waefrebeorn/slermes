/*
 * sqlite_sessions_flow.c — FLOW + leaf ports of
 * persistence/sqlite/conversation/sessions.py (SQLiteSessionRepository).
 *
 * Self-contained, libjson-only. Every function is static (ss_*); the harness
 * #includes this file and drives ss_dispatch through the universal seam:
 *   io->call(ctx, name, args) returns a NEW json_t the CALLER frees.
 *   NULL == Python None. {"__raise__": cls, "message": msg} == an exception.
 *   args is OWNED by ss_seam (freed after the call); the stub copies it for
 *   the trace. KeyError specs carry the RAW key; str(KeyError(k)) == repr(k)
 *   is applied once at exc/ret projection (ss_spec_emit).
 *
 * Seamed collaborators (outside the ported set; the oracle installs identical
 * recording stubs):
 *   refusals.enter / refusals.exit  STRUCTURAL (no script, never raise):
 *       the _errors.py refusals() CM as a passthrough; enter {session_id},
 *       exit {session_id, exc} where exc is the str-form raise spec leaving
 *       the with-body AFTER the ported _title_refusals conversion.
 *   create_session, set_session_title, set_auto_title, delete_session,
 *   delete_session_if_empty, backfill_null_session_profiles,
 *   _own_profile_name, get_session_title_source, list_recent_sessions_bounded,
 *   list_unlinked_telegram_sessions_for_user:
 *       {"args": [positionals after holder], "kwargs": {...}} with None-valued
 *       kwargs DROPPED on both sides. holder() is a silent state double.
 *   _parse_model_config {"raw"} -> config dict; is_corrupt_model_config
 *       {"raw"} -> bool; stored_instant {"value","session_id","field"} -> the
 *       decoded epoch float or null (the .timestamp() projection; null reads
 *       UNTRUSTED_INSTANT for started_at, epoch floor 0.0); session_unread
 *       {"row": copy} -> the unread flag; conn.execute {"sql","params"} ->
 *       the fetchone() row (list) or null (holder._read_ctx is a silent CM).
 *
 * Decision chains transcribed statement-for-statement: kwarg evaluation order
 * (session_record's is_corrupt seam fires at its kwarg position, AFTER the
 * counter int() coercions; the recent-row key checks interleave with the
 * started_at float()), truthiness (`or`, `if values`) vs `is None` identity
 * (_optional_list, expected_ids/expected_display), the _MARKERS first-nonblank
 * priority, _removed order-preserving dedup, the _TITLE_HELD greedy backtrack
 * (rightmost separator with a non-empty holder), create's AuthorityRefusal
 * not_found -> RetiredSessionError conversion, list_recent_sessions'
 * TimeoutError -> RecentBrowseTimeout conversion, RecentBrowseQuery clamping,
 * and BaseException passthrough at every except site (the catches match
 * ValueError / AuthorityRefusal / TimeoutError kinds only).
 *
 * Fixture domain (ASCII, shared with the oracle/generator): JSON text cells
 * are canonical strict JSON (ss_json_loads validates the Python grammar BEFORE
 * libjson's lenient parser: no leading zeros, bare '-'/'1.'/'1e', NaN/Infinity,
 * lone surrogates, raw controls, invalid escapes, trailing commas, non-JSON
 * whitespace; depth <= 32). int()/float() strings use ASCII whitespace and
 * finite values; origin/model_config values are str/bool/null/int (py_dumps
 * prints integral numbers without '.0'); counter int(x or 0) cells are
 * null/bool/int/int-string; store-verb scripts return the documented shapes.
 * SessionId is NewType(str) (identity); records project to field-order dicts
 * with enums as .value; tuples as lists. Fail-closed: a missing seam channel
 * or a missing/exhausted script raises RuntimeError through the same channel.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

typedef struct ss_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ss_io_t;

/* ── Python value semantics ── */

static bool ss_truth(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool ss_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* copy-or-None: a missing key must emit null, not a NULL pointer. */
static json_t *ss_cp(const json_t *v) { return v ? json_copy(v) : json_null(); }

static const char *ss_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* str() of a scalar for error messages (None/bool/int/float/str). */
static void ss_py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) {
                snprintf(buf, cap, "%.0f", v->num_val);
            } else {
                for (int prec = 1; prec <= 17; prec++) {
                    snprintf(buf, cap, "%.*g", prec, v->num_val);
                    if (strtod(buf, NULL) == v->num_val) break;
                }
            }
            return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        default: snprintf(buf, cap, "<%s>", ss_typename(v)); return;
    }
}

/* repr() of a string (KeyError keys, int()/float() ValueError text). */
static char *ss_py_repr(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = quote;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == (unsigned char)quote || ch == '\\') { out[o++] = '\\'; out[o++] = (char)ch; }
        else if (ch == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (ch == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (ch == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (ch < 0x20 || ch == 0x7f) { o += (size_t)sprintf(out + o, "\\x%02x", ch); }
        else out[o++] = (char)ch;
    }
    out[o++] = quote;
    out[o] = '\0';
    return out;
}

/* ── raise specs ── */

static void ss_raise(json_t **out, const char *cls, const char *msg) {
    if (!out || *out) return;
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "message", json_string(msg ? msg : ""));
    *out = r;
}

static void ss_raise_fmt(json_t **out, const char *cls, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ss_raise(out, cls, buf);
}

static bool ss_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *ss_kind(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}

/* except ValueError / AuthorityRefusal / TimeoutError never catch these. */
static bool ss_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0 || strcmp(kind, "CancelledError") == 0;
}

/* Copy a raise spec into an exc/ret projection, reprs-ing KeyError once. */
static json_t *ss_spec_emit(const json_t *spec) {
    json_t *r = json_copy(spec);
    if (strcmp(ss_kind(r), "KeyError") == 0) {
        char *rep = ss_py_repr(json_get_str(r, "message", ""));
        json_set(r, "message", json_string(rep ? rep : ""));
        free(rep);
    }
    return r;
}

/* ── universal seam ── */

/* Scripted seam: args owned here (freed after the call); result caller-freed.
 * A raise spec result transfers into *raise_out. Fail-closed without a channel. */
static json_t *ss_seam(ss_io_t *io, const char *name, json_t *args, json_t **raise_out) {
    if (!io || !io->call) {
        json_free(args);
        ss_raise(raise_out, "RuntimeError", "no seam channel");
        return NULL;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (ss_is_raise(r)) { *raise_out = r; return NULL; }
    return r;
}

/* Structural refusals events: record-only, never scripted, never raise. */
static void ss_refusals_enter(ss_io_t *io, const char *sid, json_t **raise_out) {
    json_t *a = json_object();
    json_set(a, "session_id", sid ? json_string(sid) : json_null());
    if (!io || !io->call) {
        json_free(a);
        ss_raise(raise_out, "RuntimeError", "no seam channel");
        return;
    }
    json_t *r = io->call(io->ctx, "refusals.enter", a);
    json_free(a);
    json_free(r);
}

static void ss_refusals_exit(ss_io_t *io, const char *sid, const json_t *raise_spec) {
    json_t *a = json_object();
    json_set(a, "session_id", sid ? json_string(sid) : json_null());
    json_set(a, "exc", raise_spec ? ss_spec_emit(raise_spec) : json_null());
    if (!io || !io->call) { json_free(a); return; }
    json_t *r = io->call(io->ctx, "refusals.exit", a);
    json_free(a);
    json_free(r);
}

/* ── ASCII whitespace / scalar coercions ── */

static bool ss_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/* Python str.strip() over the ASCII domain. Returns malloc'd trimmed copy. */
static char *ss_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && ss_is_ws(s[a])) a++;
    while (b > a && ss_is_ws(s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static bool ss_is_digit(char c) { return c >= '0' && c <= '9'; }

/* A digit run with PEP 515 underscores: >=1 digit, first/last digits, '_'
 * only between two digits. */
static bool ss_digits_ok(const char *s, size_t n) {
    if (n == 0 || !ss_is_digit(s[0]) || !ss_is_digit(s[n - 1])) return false;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (i == 0 || i + 1 >= n || !ss_is_digit(s[i - 1]) || !ss_is_digit(s[i + 1]))
                return false;
        } else if (!ss_is_digit(s[i])) {
            return false;
        }
    }
    return true;
}

static void ss_strip_uscore(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) if (*r != '_') *w++ = *r;
    *w = '\0';
}

/* int(v): null/bool/number/string (ASCII domain). Raises ValueError/TypeError
 * with the live messages; returns 0 on raise. */
static double ss_py_int(const json_t *v, json_t **raise_out) {
    if (!v || v->type == JSON_NULL) {
        ss_raise(raise_out, "TypeError",
                 "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return trunc(v->num_val);
    if (v->type == JSON_STRING) {
        char *t = ss_strip(v->str_val);
        char *p = t;
        if (*p == '+' || *p == '-') p++;
        if (!ss_digits_ok(p, strlen(p))) {
            char *rep = ss_py_repr(v->str_val);
            ss_raise_fmt(raise_out, "ValueError",
                         "invalid literal for int() with base 10: %s", rep ? rep : "''");
            free(rep);
            free(t);
            return 0;
        }
        ss_strip_uscore(p);
        double d = (double)strtoll(p, NULL, 10);
        if (t[0] == '-') d = -d; /* carry the consumed sign into the value */
        free(t);
        return d;
    }
    ss_raise_fmt(raise_out, "TypeError",
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 ss_typename(v));
    return 0;
}

/* float(v): finite ASCII domain (inf/nan strings are outside the fixture
 * domain and fail closed). */
static double ss_py_float(const json_t *v, json_t **raise_out) {
    if (!v || v->type == JSON_NULL) {
        ss_raise(raise_out, "TypeError",
                 "float() argument must be a string or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        char *t = ss_strip(v->str_val);
        char *p = t;
        bool ok = true;
        if (*p == '+' || *p == '-') p++;
        char *int_start = p;
        while (ss_is_digit(*p) || *p == '_') p++;
        size_t int_n = (size_t)(p - int_start);
        bool has_frac = false;
        size_t frac_n = 0;
        if (*p == '.') {
            has_frac = true;
            p++;
            char *frac_start = p;
            while (ss_is_digit(*p) || *p == '_') p++;
            frac_n = (size_t)(p - frac_start);
        }
        if (int_n == 0 && frac_n == 0) ok = false;
        if (ok && int_n > 0 && !ss_digits_ok(int_start, int_n)) ok = false;
        if (ok && has_frac && frac_n > 0) {
            /* the fractional run starts right after '.' */
            if (!ss_digits_ok(int_start + int_n + 1, frac_n)) ok = false;
        }
        if (ok && (*p == 'e' || *p == 'E')) {
            p++;
            if (*p == '+' || *p == '-') p++;
            char *exp_start = p;
            while (ss_is_digit(*p) || *p == '_') p++;
            size_t exp_n = (size_t)(p - exp_start);
            if (exp_n == 0 || !ss_digits_ok(exp_start, exp_n)) ok = false;
        }
        if (ok && *p != '\0') ok = false;
        if (!ok) {
            char *rep = ss_py_repr(v->str_val);
            ss_raise_fmt(raise_out, "ValueError",
                         "could not convert string to float: %s", rep ? rep : "''");
            free(rep);
            free(t);
            return 0;
        }
        ss_strip_uscore(t);
        double d = strtod(t, NULL);
        free(t);
        if (!isfinite(d)) {
            char *rep = ss_py_repr(v->str_val);
            ss_raise_fmt(raise_out, "ValueError",
                         "could not convert string to float: %s", rep ? rep : "''");
            free(rep);
            return 0;
        }
        return d;
    }
    ss_raise_fmt(raise_out, "TypeError",
                 "float() argument must be a string or a real number, not '%s'",
                 ss_typename(v));
    return 0;
}

/* int(x or 0) / float(x or 0.0): the truthiness `or` fires first. */
static double ss_int_or_0(const json_t *v, json_t **raise_out) {
    if (!ss_truth(v)) return 0;
    return ss_py_int(v, raise_out);
}

static double ss_float_or_0(const json_t *v, json_t **raise_out) {
    if (!ss_truth(v)) return 0.0;
    return ss_py_float(v, raise_out);
}

/* ── Python json.dumps (default separators, ensure_ascii, integral ints) ── */

typedef struct { char *b; size_t len, cap; } ss_buf_t;

static void ss_buf_putc(ss_buf_t *b, char c) {
    if (b->len + 2 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 64;
        b->b = (char *)realloc(b->b, b->cap);
    }
    b->b[b->len++] = c;
    b->b[b->len] = '\0';
}

static void ss_buf_puts(ss_buf_t *b, const char *s) {
    while (*s) ss_buf_putc(b, *s++);
}

static void ss_buf_putn(ss_buf_t *b, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) ss_buf_putc(b, s[i]);
}

static void ss_dumps_str(ss_buf_t *b, const char *s) {
    ss_buf_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '"') ss_buf_puts(b, "\\\"");
        else if (c == '\\') ss_buf_puts(b, "\\\\");
        else if (c == '\b') ss_buf_puts(b, "\\b");
        else if (c == '\f') ss_buf_puts(b, "\\f");
        else if (c == '\n') ss_buf_puts(b, "\\n");
        else if (c == '\r') ss_buf_puts(b, "\\r");
        else if (c == '\t') ss_buf_puts(b, "\\t");
        else if (c < 0x20) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "\\u%04x", c);
            ss_buf_puts(b, tmp);
        } else if (c < 0x80) {
            ss_buf_putc(b, (char)c);
        } else {
            /* UTF-8 decode -> \uXXXX (ensure_ascii); ASCII domain in fixtures */
            unsigned long cp = 0xFFFD;
            int extra = 0;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
            bool ok = extra > 0;
            for (int i = 1; i <= extra && ok; i++) {
                if ((p[i] & 0xC0) != 0x80) { ok = false; break; }
                cp = (cp << 6) | (p[i] & 0x3F);
            }
            if (ok) p += extra; else cp = 0xFFFD;
            char tmp[16];
            if (cp >= 0x10000) {
                cp -= 0x10000;
                snprintf(tmp, sizeof(tmp), "\\u%04lx\\u%04lx",
                         0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
            } else {
                snprintf(tmp, sizeof(tmp), "\\u%04lx", cp);
            }
            ss_buf_puts(b, tmp);
        }
    }
    ss_buf_putc(b, '"');
}

static void ss_dumps_node(ss_buf_t *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { ss_buf_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: ss_buf_puts(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char tmp[64];
            if (v->num_val == (double)(long long)v->num_val && fabs(v->num_val) < 1e15) {
                snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
            } else {
                for (int prec = 1; prec <= 17; prec++) {
                    snprintf(tmp, sizeof(tmp), "%.*g", prec, v->num_val);
                    if (strtod(tmp, NULL) == v->num_val) break;
                }
            }
            ss_buf_puts(b, tmp);
            return;
        }
        case JSON_STRING: ss_dumps_str(b, v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY:
            ss_buf_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ss_buf_puts(b, ", ");
                ss_dumps_node(b, v->c.items[i]);
            }
            ss_buf_putc(b, ']');
            return;
        case JSON_OBJECT:
            ss_buf_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ss_buf_puts(b, ", ");
                ss_dumps_str(b, v->c.keys[i]);
                ss_buf_puts(b, ": ");
                ss_dumps_node(b, v->c.items[i]);
            }
            ss_buf_putc(b, '}');
            return;
        default: ss_buf_puts(b, "null"); return;
    }
}

/* Python json.dumps(v) with default arguments (ASCII fixture domain). */
static char *ss_py_dumps(const json_t *v) {
    ss_buf_t b = { NULL, 0, 0 };
    ss_dumps_node(&b, v);
    if (!b.b) { b.b = (char *)malloc(1); if (b.b) b.b[0] = '\0'; }
    return b.b;
}

/* ── strict Python json.loads grammar over libjson ── */

static void ss_jws(const char **p, const char *end) {
    while (*p < end && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')) (*p)++;
}

static bool ss_jhex4(const char *p, const char *end, unsigned long *cp) {
    if (p + 4 > end) return false;
    unsigned long v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned long)(c - 'A' + 10);
        else return false;
    }
    *cp = v;
    return true;
}

static bool ss_jstring(const char **p, const char *end) {
    (*p)++; /* opening quote */
    while (*p < end && **p != '"') {
        unsigned char c = (unsigned char)**p;
        if (c < 0x20) return false; /* raw control: Python strict rejects */
        if (c == '\\') {
            (*p)++;
            if (*p >= end) return false;
            char e = **p;
            if (e == 'u') {
                unsigned long cp;
                if (!ss_jhex4(*p + 1, end, &cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDFFF) return false; /* lone surrogates out of domain */
                *p += 4;
            } else if (e != '"' && e != '\\' && e != '/' && e != 'b' && e != 'f'
                       && e != 'n' && e != 'r' && e != 't') {
                return false;
            }
        }
        (*p)++;
    }
    if (*p >= end) return false;
    (*p)++; /* closing quote */
    return true;
}

static bool ss_jnumber(const char **p, const char *end) {
    const char *s = *p;
    if (*p < end && **p == '-') (*p)++;
    if (*p >= end) return false;
    if (**p == '0') {
        (*p)++;
        if (*p < end && ss_is_digit(**p)) return false; /* leading zero */
    } else if (ss_is_digit(**p)) {
        while (*p < end && ss_is_digit(**p)) (*p)++;
    } else {
        return false;
    }
    if (*p < end && **p == '.') {
        (*p)++;
        if (*p >= end || !ss_is_digit(**p)) return false;
        while (*p < end && ss_is_digit(**p)) (*p)++;
    }
    if (*p < end && (**p == 'e' || **p == 'E')) {
        (*p)++;
        if (*p < end && (**p == '+' || **p == '-')) (*p)++;
        if (*p >= end || !ss_is_digit(**p)) return false;
        while (*p < end && ss_is_digit(**p)) (*p)++;
    }
    return *p > s;
}

static bool ss_jvalue(const char **p, const char *end, int depth) {
    if (depth > 32) return false;
    ss_jws(p, end);
    if (*p >= end) return false;
    char c = **p;
    if (c == '"') return ss_jstring(p, end);
    if (c == 't') {
        if ((size_t)(end - *p) < 4 || strncmp(*p, "true", 4) != 0) return false;
        *p += 4; return true;
    }
    if (c == 'f') {
        if ((size_t)(end - *p) < 5 || strncmp(*p, "false", 5) != 0) return false;
        *p += 5; return true;
    }
    if (c == 'n') {
        if ((size_t)(end - *p) < 4 || strncmp(*p, "null", 4) != 0) return false;
        *p += 4; return true;
    }
    if (c == '-' || ss_is_digit(c)) return ss_jnumber(p, end);
    if (c == '[') {
        (*p)++;
        ss_jws(p, end);
        if (*p < end && **p == ']') { (*p)++; return true; }
        for (;;) {
            if (!ss_jvalue(p, end, depth + 1)) return false;
            ss_jws(p, end);
            if (*p >= end) return false;
            if (**p == ']') { (*p)++; return true; }
            if (**p != ',') return false;
            (*p)++;
        }
    }
    if (c == '{') {
        (*p)++;
        ss_jws(p, end);
        if (*p < end && **p == '}') { (*p)++; return true; }
        for (;;) {
            ss_jws(p, end);
            if (*p >= end || **p != '"') return false;
            if (!ss_jstring(p, end)) return false;
            ss_jws(p, end);
            if (*p >= end || **p != ':') return false;
            (*p)++;
            if (!ss_jvalue(p, end, depth + 1)) return false;
            ss_jws(p, end);
            if (*p >= end) return false;
            if (**p == '}') { (*p)++; return true; }
            if (**p != ',') return false;
            (*p)++;
        }
    }
    return false;
}

/* json.loads(text) for the ASCII strict domain: validate the Python grammar,
 * then build the tree with libjson (whose leniency never fires on validated
 * text). NULL == ValueError (the _json_value catch). */
static json_t *ss_json_loads(const char *text) {
    if (!text) return NULL;
    const char *p = text, *end = text + strlen(text);
    if (!ss_jvalue(&p, end, 0)) return NULL;
    ss_jws(&p, end);
    if (p != end) return NULL;
    char *err = NULL;
    json_t *v = json_parse(text, &err);
    free(err);
    return v;
}

/* ── ported leaves ── */

/* PoP: ss_stripped @ persistence/sqlite/conversation/sessions.py:_stripped */
static json_t *ss_stripped(const json_t *value) {
    if (!value || value->type != JSON_STRING) return NULL;
    char *t = ss_strip(value->str_val);
    json_t *r = (t && t[0]) ? json_string(t) : NULL;
    free(t);
    return r;
}

/* PoP: ss_json_value @ persistence/sqlite/conversation/sessions.py:_json_value */
static json_t *ss_json_value(const json_t *text) {
    if (!text || text->type != JSON_STRING || !text->str_val || !text->str_val[0])
        return NULL;
    return ss_json_loads(text->str_val);
}

/* PoP: ss_tool_pin @ persistence/sqlite/conversation/sessions.py:_tool_pin */
static json_t *ss_tool_pin(const json_t *text) {
    if (text && text->type == JSON_STRING && text->str_val) {
        const char *s = text->str_val;
        size_t n = strlen(s);
        if (n == 64) {
            bool hex = true;
            for (size_t i = 0; i < n; i++) {
                char c = s[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { hex = false; break; }
            }
            if (hex) return NULL; /* unresolved content hash */
        }
    }
    return ss_json_value(text);
}

/* PoP: ss_ids @ persistence/sqlite/conversation/sessions.py:_ids */
static json_t *ss_ids(const json_t *values, json_t **raise_out) {
    if (!values || values->type == JSON_NULL) {
        ss_raise(raise_out, "TypeError", "'NoneType' object is not iterable");
        return NULL;
    }
    if (values->type != JSON_ARRAY) {
        ss_raise_fmt(raise_out, "TypeError", "'%s' object is not iterable", ss_typename(values));
        return NULL;
    }
    return json_copy(values); /* SessionId is NewType(str): identity */
}

/* PoP: ss_removed @ persistence/sqlite/conversation/sessions.py:_removed */
static json_t *ss_removed(const json_t *values, json_t **raise_out) {
    if (!values || values->type == JSON_NULL) {
        ss_raise(raise_out, "TypeError", "'NoneType' object is not iterable");
        return NULL;
    }
    if (values->type != JSON_ARRAY) {
        ss_raise_fmt(raise_out, "TypeError", "'%s' object is not iterable", ss_typename(values));
        return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < values->c.count; i++) {
        const json_t *v = values->c.items[i];
        bool dup = false;
        for (size_t j = 0; j < out->c.count && !dup; j++) {
            const json_t *o = out->c.items[j];
            if (v->type == JSON_STRING && o->type == JSON_STRING)
                dup = strcmp(v->str_val, o->str_val) == 0;
            else
                dup = v->type == o->type && v->type == JSON_NULL;
        }
        if (!dup) json_append(out, json_copy(v));
    }
    return out;
}

/* PoP: ss_listed @ persistence/sqlite/conversation/sessions.py:_listed */
static json_t *ss_listed(const json_t *values) {
    return ss_truth(values) ? json_copy(values) : NULL;
}

/* PoP: ss_optional_list @ persistence/sqlite/conversation/sessions.py:_optional_list */
static json_t *ss_optional_list(const json_t *values) {
    return ss_is_null(values) ? NULL : json_copy(values);
}

/* PoP: ss_sql_limit @ persistence/sqlite/conversation/sessions.py:_sql_limit */
static json_t *ss_sql_limit(const json_t *window, json_t **raise_out) {
    const json_t *limit = window ? json_obj_get(window, "limit") : NULL;
    const json_t *offset_n = window ? json_obj_get(window, "offset") : NULL;
    double offset = offset_n ? json_get_num(window, "offset", 0) : 0;
    bool limit_neg = limit && limit->type == JSON_NUMBER && limit->num_val < 0;
    if (limit_neg || offset < 0) {
        char lbuf[64], obuf[64];
        ss_py_str(limit, lbuf, sizeof(lbuf));
        if (offset_n) ss_py_str(offset_n, obuf, sizeof(obuf));
        else snprintf(obuf, sizeof(obuf), "0");
        ss_raise_fmt(raise_out, "ValueError",
                     "SessionWindow needs limit >= 0 and offset >= 0, got %s, %s", lbuf, obuf);
        return NULL;
    }
    return json_number(limit && limit->type != JSON_NULL ? limit->num_val : -1);
}

/* PoP: ss_last_active @ persistence/sqlite/conversation/sessions.py:_last_active */
static json_t *ss_last_active(const json_t *row, json_t **raise_out) {
    const json_t *v = row ? json_obj_get(row, "last_active") : NULL;
    if (ss_is_null(v)) return NULL;
    double d = ss_py_float(v, raise_out);
    if (*raise_out) return NULL;
    return json_number(d);
}

/* The RosterSession field set, in dataclass order. */
static const char *SS_ROSTER_FIELDS[] = {
    "id", "resolved_id", "title", "root_title", "preview",
    "started_at", "last_active", "message_count",
};

/* TypeError "missing N required positional arguments" (CPython formatting:
 * 2 -> "'a' and 'b'"; 3+ -> "'a', 'b', and 'c'"). */
static void ss_missing_args(json_t **raise_out, const char *cls,
                            const char **names, size_t n) {
    char buf[512];
    size_t o = 0;
    o += (size_t)snprintf(buf + o, sizeof(buf) - o,
                          "%s.__init__() missing %zu required positional argument%s: ",
                          cls, n, n == 1 ? "" : "s");
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            if (n == 2) o += (size_t)snprintf(buf + o, sizeof(buf) - o, " and ");
            else if (i + 1 == n) o += (size_t)snprintf(buf + o, sizeof(buf) - o, ", and ");
            else o += (size_t)snprintf(buf + o, sizeof(buf) - o, ", ");
        }
        o += (size_t)snprintf(buf + o, sizeof(buf) - o, "'%s'", names[i]);
    }
    ss_raise(raise_out, "TypeError", buf);
}

/* PoP: ss_roster_session @ persistence/sqlite/conversation/sessions.py:_roster_session */
static json_t *ss_roster_session(const json_t *fields, json_t **raise_out) {
    if (ss_is_null(fields)) return NULL;
    if (fields->type != JSON_OBJECT) {
        ss_raise_fmt(raise_out, "TypeError", "argument of type '%s' is not a mapping",
                     ss_typename(fields));
        return NULL;
    }
    /* {**fields, "id": fields["id"], "resolved_id": fields["resolved_id"]} */
    if (!json_has(fields, "id")) { ss_raise(raise_out, "KeyError", "id"); return NULL; }
    if (!json_has(fields, "resolved_id")) { ss_raise(raise_out, "KeyError", "resolved_id"); return NULL; }
    size_t nf = sizeof(SS_ROSTER_FIELDS) / sizeof(SS_ROSTER_FIELDS[0]);
    for (size_t i = 0; i < fields->c.count; i++) {
        const char *k = fields->c.keys[i];
        bool known = false;
        for (size_t j = 0; j < nf && !known; j++) known = strcmp(k, SS_ROSTER_FIELDS[j]) == 0;
        if (!known) {
            ss_raise_fmt(raise_out, "TypeError",
                         "RosterSession.__init__() got an unexpected keyword argument '%s'", k);
            return NULL;
        }
    }
    const char *missing[8];
    size_t nm = 0;
    for (size_t j = 0; j < nf; j++)
        if (!json_has(fields, SS_ROSTER_FIELDS[j])) missing[nm++] = SS_ROSTER_FIELDS[j];
    if (nm > 0) { ss_missing_args(raise_out, "RosterSession", missing, nm); return NULL; }
    json_t *out = json_object();
    for (size_t j = 0; j < nf; j++)
        json_set(out, SS_ROSTER_FIELDS[j], json_copy(json_obj_get(fields, SS_ROSTER_FIELDS[j])));
    return out;
}

/* ── session_record ── */

typedef enum { SS_GET, SS_GET0, SS_BOOL, SS_INT0, SS_SPEC } ss_field_kind_t;

/* SessionRecord's 74 fields in dataclass order; SS_SPEC values are computed
 * by the mapper (markers, instants, seams) before/at the field's position. */
static const struct { const char *name; ss_field_kind_t kind; } SS_REC_FIELDS[] = {
    {"session_id", SS_SPEC}, {"source", SS_SPEC}, {"created_source", SS_GET},
    {"user_id", SS_GET}, {"session_key", SS_GET}, {"chat_id", SS_GET},
    {"chat_type", SS_GET}, {"thread_id", SS_GET}, {"display_name", SS_GET},
    {"origin", SS_SPEC}, {"expiry_finalized", SS_BOOL}, {"model", SS_GET},
    {"model_config", SS_SPEC}, {"system_prompt", SS_GET}, {"system_prompt_hash", SS_GET},
    {"parent_session_id", SS_GET}, {"lineage_kind", SS_SPEC}, {"reset_from", SS_SPEC},
    {"branched_from", SS_SPEC}, {"started_at", SS_SPEC}, {"ended_at", SS_SPEC},
    {"end_reason", SS_GET}, {"message_count", SS_GET0}, {"tool_call_count", SS_GET0},
    {"input_tokens", SS_GET0}, {"output_tokens", SS_GET0}, {"cache_read_tokens", SS_GET0},
    {"cache_write_tokens", SS_GET0}, {"reasoning_tokens", SS_GET0}, {"api_call_count", SS_GET0},
    {"estimated_cost_usd", SS_GET}, {"actual_cost_usd", SS_GET}, {"billing_provider", SS_GET},
    {"billing_base_url", SS_GET}, {"billing_mode", SS_GET}, {"cost_status", SS_GET},
    {"cost_source", SS_GET}, {"pricing_version", SS_GET}, {"cwd", SS_GET},
    {"git_branch", SS_GET}, {"git_repo_root", SS_GET}, {"git_metadata_generation", SS_INT0},
    {"title", SS_GET}, {"title_source", SS_SPEC}, {"last_activity_at", SS_GET},
    {"last_activity_description", SS_GET}, {"last_activity_provenance", SS_GET},
    {"handoff_state", SS_GET}, {"handoff_platform", SS_GET}, {"handoff_error", SS_GET},
    {"compression_failure_cooldown_until", SS_GET}, {"compression_failure_error", SS_GET},
    {"compression_fallback_streak", SS_INT0}, {"compression_ineffective_count", SS_INT0},
    {"compression_recovery_deadline", SS_GET}, {"compression_overload_streak", SS_INT0},
    {"profile_name", SS_GET}, {"transport_profile", SS_GET}, {"rewind_count", SS_INT0},
    {"archived", SS_BOOL}, {"auto_archived", SS_BOOL}, {"pinned", SS_BOOL},
    {"hidden", SS_BOOL}, {"last_read_at", SS_GET}, {"tool_pin", SS_SPEC},
    {"transcript_revision", SS_INT0}, {"runtime_revision", SS_INT0},
    {"runtime_generation", SS_INT0}, {"lineage_origin_id", SS_SPEC},
    {"model_config_corrupt", SS_SPEC}, {"model_config_json", SS_GET},
    {"origin_json", SS_GET}, {"tool_names", SS_GET}, {"prompt_cache_clock", SS_GET},
};

/* The fork markers in first-non-blank priority order. */
static const struct { const char *key; const char *kind; } SS_MARKERS[] = {
    {"_delegate_from", "delegate"},
    {"_branched_from", "branch"},
    {"_reset_from", "reset"},
};

static const char *ss_title_source_map(const json_t *v) {
    if (!v || v->type != JSON_STRING) return NULL;
    const char *s = v->str_val;
    if (strcmp(s, "derived") == 0 || strcmp(s, "llm") == 0 || strcmp(s, "user") == 0) return s;
    return NULL;
}

/* PoP: ss_session_record @ persistence/sqlite/conversation/sessions.py:session_record */
static json_t *ss_session_record(const json_t *row, ss_io_t *io, json_t **raise_out) {
    if (!row || row->type != JSON_OBJECT) {
        ss_raise_fmt(raise_out, "AttributeError", "'%s' object has no attribute 'get'",
                     ss_typename(row));
        return NULL;
    }
    /* config = _parse_model_config(row.get("model_config")) */
    json_t *ca = json_object();
    json_set(ca, "raw", ss_cp(json_obj_get(row, "model_config")));
    json_t *config = ss_seam(io, "_parse_model_config", ca, raise_out);
    if (*raise_out) return NULL;
    if (!config || config->type != JSON_OBJECT) {
        ss_raise_fmt(raise_out, "AttributeError", "'%s' object has no attribute 'get'",
                     ss_typename(config));
        json_free(config);
        return NULL;
    }
    /* marker loop: first non-blank _stripped(config.get(key)) names the kind;
     * origin_id is the RAW config value, not the stripped one. */
    const char *kind = "none";
    json_t *origin_id = NULL;
    for (size_t i = 0; i < 3; i++) {
        json_t *stripped = ss_stripped(json_obj_get(config, SS_MARKERS[i].key));
        if (stripped) {
            kind = SS_MARKERS[i].kind;
            origin_id = json_copy(json_obj_get(config, SS_MARKERS[i].key));
            json_free(stripped);
            break;
        }
    }
    json_t *reset_from = ss_stripped(json_obj_get(config, "_reset_from"));
    json_t *branched_from = ss_stripped(json_obj_get(config, "_branched_from"));
    /* origin = dict(origin) if isinstance(origin, Mapping) else None */
    json_t *origin_decoded = ss_json_value(json_obj_get(row, "origin_json"));
    json_t *origin = NULL;
    if (origin_decoded && origin_decoded->type == JSON_OBJECT) origin = json_copy(origin_decoded);
    json_free(origin_decoded);
    /* row["id"] is read for the stored_instant session_id kwarg BEFORE the
     * first instant seam fires. */
    if (!json_has(row, "id")) {
        ss_raise(raise_out, "KeyError", "id");
        goto fail;
    }
    const char *sid = json_get_str(row, "id", "");
    json_t *sa = json_object();
    json_set(sa, "value", ss_cp(json_obj_get(row, "started_at")));
    json_set(sa, "session_id", json_string(sid));
    json_set(sa, "field", json_string("started_at"));
    json_t *started = ss_seam(io, "stored_instant", sa, raise_out);
    if (*raise_out) goto fail;
    json_t *ea = json_object();
    json_set(ea, "value", ss_cp(json_obj_get(row, "ended_at")));
    json_set(ea, "session_id", json_string(sid));
    json_set(ea, "field", json_string("ended_at"));
    json_t *ended = ss_seam(io, "stored_instant", ea, raise_out);
    if (*raise_out) { json_free(started); goto fail; }
    /* (started or UNTRUSTED_INSTANT).timestamp(): the floor epoch is 0.0 */
    json_t *started_at = json_number(!ss_is_null(started) ? started->num_val : 0.0);
    json_t *ended_at = ss_is_null(ended) ? json_null() : json_number(ended->num_val);
    json_free(started);
    json_free(ended);
    json_t *title_source = NULL;
    {
        const char *ts = ss_title_source_map(json_obj_get(row, "title_source"));
        if (ts) title_source = json_string(ts);
    }
    json_t *tool_pin = ss_tool_pin(json_obj_get(row, "tool_names"));
    json_t *out = json_object();
    size_t nf = sizeof(SS_REC_FIELDS) / sizeof(SS_REC_FIELDS[0]);
    for (size_t i = 0; i < nf; i++) {
        const char *name = SS_REC_FIELDS[i].name;
        /* model_config_json carries the stored model_config cell. */
        const json_t *cell = json_obj_get(row,
            strcmp(name, "model_config_json") == 0 ? "model_config" : name);
        json_t *val = NULL;
        switch (SS_REC_FIELDS[i].kind) {
            case SS_GET: val = ss_cp(cell); break;
            case SS_GET0: val = cell ? json_copy(cell) : json_number(0); break;
            case SS_BOOL: val = json_bool(ss_truth(cell)); break;
            case SS_INT0: {
                double d = ss_int_or_0(cell, raise_out);
                if (*raise_out) { json_free(out); out = NULL; goto fail; }
                val = json_number(d);
                break;
            }
            case SS_SPEC:
                if (strcmp(name, "session_id") == 0) val = json_string(sid);
                else if (strcmp(name, "source") == 0)
                    val = ss_truth(cell) ? json_copy(cell) : json_string("");
                else if (strcmp(name, "origin") == 0) val = origin ? json_copy(origin) : json_null();
                else if (strcmp(name, "model_config") == 0) val = json_copy(config);
                else if (strcmp(name, "lineage_kind") == 0) val = json_string(kind);
                else if (strcmp(name, "reset_from") == 0) val = reset_from ? json_copy(reset_from) : json_null();
                else if (strcmp(name, "branched_from") == 0) val = branched_from ? json_copy(branched_from) : json_null();
                else if (strcmp(name, "started_at") == 0) val = json_copy(started_at);
                else if (strcmp(name, "ended_at") == 0) val = json_copy(ended_at);
                else if (strcmp(name, "title_source") == 0) val = title_source ? json_copy(title_source) : json_null();
                else if (strcmp(name, "tool_pin") == 0) val = tool_pin ? json_copy(tool_pin) : json_null();
                else if (strcmp(name, "lineage_origin_id") == 0) val = origin_id ? json_copy(origin_id) : json_null();
                else if (strcmp(name, "model_config_corrupt") == 0) {
                    /* the is_corrupt kwarg fires at its construction position,
                     * AFTER the counter int() coercions above. */
                    json_t *ia = json_object();
                    json_set(ia, "raw", ss_cp(json_obj_get(row, "model_config")));
                    json_t *corrupt = ss_seam(io, "is_corrupt_model_config", ia, raise_out);
                    if (*raise_out) { json_free(out); out = NULL; goto fail; }
                    val = json_bool(ss_truth(corrupt));
                    json_free(corrupt);
                }
                break;
        }
        json_set(out, name, val);
    }
    json_free(config); json_free(origin_id); json_free(reset_from); json_free(branched_from);
    json_free(origin); json_free(started_at); json_free(ended_at);
    json_free(title_source); json_free(tool_pin);
    return out;
fail:
    json_free(config); json_free(origin_id); json_free(reset_from); json_free(branched_from);
    json_free(origin);
    return NULL;
}

/* PoP: ss_list_entry @ persistence/sqlite/conversation/sessions.py:_list_entry */
static json_t *ss_list_entry(const json_t *row, bool compact, ss_io_t *io, json_t **raise_out) {
    json_t *session = ss_session_record(row, io, raise_out);
    if (*raise_out) return NULL;
    json_t *out = json_object();
    json_set(out, "session", session);
    const json_t *preview = json_obj_get(row, "preview");
    json_set(out, "preview", ss_truth(preview) ? json_copy(preview) : json_string(""));
    if (json_has(row, "unread")) {
        json_set(out, "unread", json_bool(ss_truth(json_obj_get(row, "unread"))));
    } else {
        json_t *ua = json_object();
        json_set(ua, "row", json_copy(row));
        json_t *unread = ss_seam(io, "session_unread", ua, raise_out);
        if (*raise_out) { json_free(out); return NULL; }
        json_set(out, "unread", ss_cp(unread));
        json_free(unread);
    }
    json_t *la = ss_last_active(row, raise_out);
    if (*raise_out) { json_free(out); return NULL; }
    json_set(out, "last_active", la ? la : json_null());
    json_set(out, "lineage_root_id", ss_cp(json_obj_get(row, "_lineage_root_id")));
    const json_t *lids = json_obj_get(row, "_lineage_ids");
    json_t *ids_in = ss_truth(lids) ? json_copy(lids) : json_array();
    json_t *ids = ss_ids(ids_in, raise_out);
    json_free(ids_in);
    if (*raise_out) { json_free(out); return NULL; }
    json_set(out, "lineage_ids", ids);
    json_set(out, "continuation_kind", ss_cp(json_obj_get(row, "continuation_kind")));
    json_set(out, "compact", json_bool(compact));
    return out;
}

/* PoP: ss_recency_entry @ persistence/sqlite/conversation/sessions.py:_recency_entry */
static json_t *ss_recency_entry(const json_t *row, ss_io_t *io, json_t **raise_out) {
    json_t *session = ss_session_record(row, io, raise_out);
    if (*raise_out) return NULL;
    json_t *la = ss_last_active(row, raise_out);
    if (*raise_out) { json_free(session); return NULL; }
    json_t *out = json_object();
    json_set(out, "session", session);
    json_set(out, "last_active", la ? la : json_null());
    return out;
}

/* ── _title_refusals ── */

#define SS_TITLE_PREFIX "Title '"
#define SS_TITLE_SEP "' is already in use by session "
#define SS_CANONICAL_PREFIX "This is the bot's canonical Bot Chat"
/* CanonicalTitleLocked's str(exc) (the em-dash is U+2014, UTF-8 encoded). */
#define SS_CANONICAL_MSG \
    "This is the bot's canonical Bot Chat \xe2\x80\x94 its name is its identity, " \
    "and renaming it would orphan the conversation. To start fresh, create a new bot instead."

/* The _TITLE_HELD fullmatch with greedy .* backtracking: the RIGHTMOST
 * separator whose holder suffix is non-empty wins. */
static bool ss_title_held(const char *text, char **title, char **holder) {
    size_t n = strlen(text);
    size_t plen = strlen(SS_TITLE_PREFIX);
    size_t slen = strlen(SS_TITLE_SEP);
    if (n < plen || strncmp(text, SS_TITLE_PREFIX, plen) != 0) return false;
    if (n < slen) return false;
    for (size_t i = n - slen + 1; i > plen; i--) {
        size_t at = i - 1;
        if (strncmp(text + at, SS_TITLE_SEP, slen) == 0 && at + slen < n) {
            *title = (char *)malloc(at - plen + 1);
            memcpy(*title, text + plen, at - plen);
            (*title)[at - plen] = '\0';
            *holder = strdup(text + at + slen);
            return true;
        }
    }
    return false;
}

/* The ValueError conversion inside _title_refusals; other kinds (incl. the
 * BaseException trio) pass through unchanged. Returns a NEW spec. */
static json_t *ss_title_transform(const json_t *spec) {
    if (strcmp(ss_kind(spec), "ValueError") != 0) return json_copy(spec);
    const char *text = json_get_str(spec, "message", "");
    char *title = NULL, *holder = NULL;
    if (ss_title_held(text, &title, &holder)) {
        json_t *r = json_object();
        json_set(r, "__raise__", json_string("TitleConflict"));
        char *msg = (char *)malloc(strlen(title) + strlen(holder) + 64);
        sprintf(msg, "Title '%s' is already in use by session %s", title, holder);
        json_set(r, "message", json_string(msg));
        free(msg); free(title); free(holder);
        return r;
    }
    if (strncmp(text, SS_CANONICAL_PREFIX, strlen(SS_CANONICAL_PREFIX)) == 0) {
        json_t *r = json_object();
        json_set(r, "__raise__", json_string("CanonicalTitleLocked"));
        json_set(r, "message", json_string(SS_CANONICAL_MSG));
        return r;
    }
    return json_copy(spec);
}

/* PoP: ss_title_refusals @ persistence/sqlite/conversation/sessions.py:_title_refusals */
/* Driven as the CM around a scripted body: body {"ret": X} returns X;
 * body {"raise": spec} raises through the conversion. */
static json_t *ss_title_refusals(const json_t *body, json_t **raise_out) {
    const json_t *rspec = body ? json_obj_get(body, "raise") : NULL;
    if (rspec) {
        *raise_out = ss_title_transform(rspec);
        return NULL;
    }
    return body ? ss_cp(json_obj_get(body, "ret")) : NULL;
}

/* ── repository flows ── */

static const char *SS_NEWSESSION_FIELDS[] = {
    "session_id", "source", "model", "cwd", "parent_session_id", "lineage_kind",
    "session_key", "chat_id", "chat_type", "thread_id", "user_id", "git_repo_root",
    "model_config", "system_prompt", "display_name", "origin", "transport_profile",
    "profile_name",
};

static const char *ss_lineage_marker(const char *kind) {
    if (strcmp(kind, "delegate") == 0) return "_delegate_from";
    if (strcmp(kind, "branch") == 0) return "_branched_from";
    if (strcmp(kind, "reset") == 0) return "_reset_from";
    return NULL;
}

/* Enum-value validation message: "'x' is not a valid Cls" for strings. */
static void ss_enum_invalid(json_t **raise_out, const char *cls, const json_t *v) {
    if (v && v->type == JSON_STRING) {
        char *rep = ss_py_repr(v->str_val);
        ss_raise_fmt(raise_out, "ValueError", "%s is not a valid %s", rep ? rep : "''", cls);
        free(rep);
    } else {
        char buf[64];
        ss_py_str(v, buf, sizeof(buf));
        ss_raise_fmt(raise_out, "ValueError", "%s is not a valid %s", buf, cls);
    }
}

/* Add a kwarg unless its value is None (both sides drop None-valued kwargs). */
static void ss_kw(json_t *kwargs, const char *key, const json_t *v) {
    if (!ss_is_null(v)) json_set(kwargs, key, json_copy(v));
}

/* PoP: ss_create @ persistence/sqlite/conversation/sessions.py:create */
static json_t *ss_create(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const json_t *spec = json_obj_get(in, "spec");
    if (spec && spec->type != JSON_OBJECT) spec = NULL;
    size_t nf = sizeof(SS_NEWSESSION_FIELDS) / sizeof(SS_NEWSESSION_FIELDS[0]);
    /* NewSession(**spec): unknown kwarg, then missing required, then the
     * lineage_kind enum conversion, then __post_init__. */
    if (spec) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *k = spec->c.keys[i];
            bool known = false;
            for (size_t j = 0; j < nf && !known; j++) known = strcmp(k, SS_NEWSESSION_FIELDS[j]) == 0;
            if (!known) {
                ss_raise_fmt(raise_out, "TypeError",
                             "NewSession.__init__() got an unexpected keyword argument '%s'", k);
                return NULL;
            }
        }
    }
    const char *missing[2];
    size_t nm = 0;
    if (!spec || !json_has(spec, "session_id")) missing[nm++] = "session_id";
    if (!spec || !json_has(spec, "source")) missing[nm++] = "source";
    if (nm > 0) { ss_missing_args(raise_out, "NewSession", missing, nm); return NULL; }
    const json_t *lk = json_obj_get(spec, "lineage_kind");
    const char *kind = "none";
    if (lk && lk->type != JSON_NULL) {
        if (lk->type != JSON_STRING
            || (strcmp(lk->str_val, "none") != 0 && strcmp(lk->str_val, "delegate") != 0
                && strcmp(lk->str_val, "branch") != 0 && strcmp(lk->str_val, "reset") != 0)) {
            ss_enum_invalid(raise_out, "SessionLineageKind", lk);
            return NULL;
        }
        kind = lk->str_val;
    }
    const json_t *sid_n = json_obj_get(spec, "session_id");
    const json_t *src_n = json_obj_get(spec, "source");
    const json_t *parent = json_obj_get(spec, "parent_session_id");
    if (!ss_truth(sid_n)) {
        ss_raise(raise_out, "ValueError", "NewSession.session_id must be non-empty");
        return NULL;
    }
    if (!ss_truth(src_n)) {
        ss_raise(raise_out, "ValueError", "NewSession.source must be non-empty");
        return NULL;
    }
    if (strcmp(kind, "none") != 0 && !ss_truth(parent)) {
        ss_raise_fmt(raise_out, "ValueError", "lineage_kind '%s' requires parent_session_id", kind);
        return NULL;
    }
    const char *sid = sid_n->str_val;
    const char *source = src_n->str_val;
    /* config = dict(spec.model_config or {}); the marker names the parent. */
    const json_t *mc = json_obj_get(spec, "model_config");
    json_t *config = ss_truth(mc) ? json_copy(mc) : json_object();
    const char *marker = ss_lineage_marker(kind);
    if (marker) json_set(config, marker, ss_cp(parent));
    /* origin_json = json.dumps(dict(spec.origin)) if spec.origin else None */
    const json_t *origin = json_obj_get(spec, "origin");
    char *origin_json = NULL;
    if (ss_truth(origin)) origin_json = ss_py_dumps(origin);
    ss_refusals_enter(io, sid, raise_out);
    if (*raise_out) { json_free(config); free(origin_json); return NULL; }
    json_t *args = json_object();
    json_t *pos = json_array();
    json_append(pos, json_string(sid));
    json_append(pos, json_string(source));
    json_set(args, "args", pos);
    json_t *kwargs = json_object();
    ss_kw(kwargs, "model", json_obj_get(spec, "model"));
    if (ss_truth(config)) json_set(kwargs, "model_config", json_copy(config));
    ss_kw(kwargs, "system_prompt", json_obj_get(spec, "system_prompt"));
    ss_kw(kwargs, "user_id", json_obj_get(spec, "user_id"));
    ss_kw(kwargs, "session_key", json_obj_get(spec, "session_key"));
    ss_kw(kwargs, "chat_id", json_obj_get(spec, "chat_id"));
    ss_kw(kwargs, "chat_type", json_obj_get(spec, "chat_type"));
    ss_kw(kwargs, "thread_id", json_obj_get(spec, "thread_id"));
    ss_kw(kwargs, "parent_session_id", parent);
    ss_kw(kwargs, "cwd", json_obj_get(spec, "cwd"));
    ss_kw(kwargs, "profile_name", json_obj_get(spec, "profile_name"));
    ss_kw(kwargs, "git_repo_root", json_obj_get(spec, "git_repo_root"));
    if (origin_json) json_set(kwargs, "origin_json", json_string(origin_json));
    ss_kw(kwargs, "display_name", json_obj_get(spec, "display_name"));
    ss_kw(kwargs, "transport_profile", json_obj_get(spec, "transport_profile"));
    json_set(args, "kwargs", kwargs);
    json_t *r = ss_seam(io, "create_session", args, raise_out);
    json_free(r);
    /* except AuthorityRefusal: not_found -> RetiredSessionError; else re-raise.
     * Other kinds (incl. BaseException) pass through. */
    if (*raise_out && strcmp(ss_kind(*raise_out), "AuthorityRefusal") == 0
        && strcmp(json_get_str(*raise_out, "message", ""), "not_found") == 0) {
        json_free(*raise_out);
        *raise_out = NULL;
        ss_raise(raise_out, "RetiredSessionError", "not_found");
    }
    ss_refusals_exit(io, sid, *raise_out);
    json_free(config);
    free(origin_json);
    if (*raise_out) return NULL;
    return json_string(sid);
}

/* set_title / set_auto_title share the refusals + _title_refusals composition. */
static json_t *ss_title_flow(ss_io_t *io, const char *sid, const json_t *title,
                             const char *verb, const char *source_value, json_t **raise_out) {
    ss_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *args = json_object();
    json_t *pos = json_array();
    json_append(pos, json_string(sid));
    json_append(pos, ss_cp(title));
    json_set(args, "args", pos);
    json_t *kwargs = json_object();
    if (source_value) json_set(kwargs, "source", json_string(source_value));
    json_set(args, "kwargs", kwargs);
    json_t *r = ss_seam(io, verb, args, raise_out);
    if (*raise_out) {
        json_t *converted = ss_title_transform(*raise_out);
        json_free(*raise_out);
        *raise_out = converted;
    }
    ss_refusals_exit(io, sid, *raise_out);
    if (*raise_out) { json_free(r); return NULL; }
    json_t *ret = json_bool(ss_truth(r));
    json_free(r);
    return ret;
}

/* PoP: ss_set_title @ persistence/sqlite/conversation/sessions.py:set_title */
static json_t *ss_set_title(const json_t *in, ss_io_t *io, json_t **raise_out) {
    return ss_title_flow(io, json_get_str(in, "session_id", ""),
                         json_obj_get(in, "title"), "set_session_title", NULL, raise_out);
}

/* PoP: ss_set_auto_title @ persistence/sqlite/conversation/sessions.py:set_auto_title */
static json_t *ss_set_auto_title(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const json_t *src = json_obj_get(in, "source");
    if (!src || src->type != JSON_STRING
        || (strcmp(src->str_val, "derived") != 0 && strcmp(src->str_val, "llm") != 0
            && strcmp(src->str_val, "user") != 0)) {
        ss_enum_invalid(raise_out, "TitleSource", src);
        return NULL;
    }
    return ss_title_flow(io, json_get_str(in, "session_id", ""),
                         json_obj_get(in, "title"), "set_auto_title", src->str_val, raise_out);
}

/* PoP: ss_delete @ persistence/sqlite/conversation/sessions.py:delete */
static json_t *ss_delete(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const char *sid = json_get_str(in, "session_id", "");
    ss_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *args = json_object();
    json_t *pos = json_array();
    json_append(pos, json_string(sid));
    json_set(args, "args", pos);
    json_t *kwargs = json_object();
    const json_t *eids = json_obj_get(in, "expected_ids");
    if (!ss_is_null(eids)) json_set(kwargs, "expected_delete_ids", json_copy(eids));
    const json_t *edisp = json_obj_get(in, "expected_display");
    if (!ss_is_null(edisp)) {
        /* {sid: [dict(message) for message in messages]} */
        json_t *disp = json_object();
        if (edisp->type == JSON_OBJECT) {
            for (size_t i = 0; i < edisp->c.count; i++) {
                const json_t *msgs = edisp->c.items[i];
                json_t *copy_msgs = json_array();
                if (msgs && msgs->type == JSON_ARRAY) {
                    for (size_t j = 0; j < msgs->c.count; j++)
                        json_append(copy_msgs, json_copy(msgs->c.items[j]));
                }
                json_set(disp, edisp->c.keys[i], copy_msgs);
            }
        }
        json_set(kwargs, "expected_display_messages", disp);
    }
    json_set(kwargs, "exclude_active_write_guards", json_bool(json_get_bool(in, "refuse_guarded", false)));
    json_set(kwargs, "include_compression_chain", json_bool(json_get_bool(in, "include_compression_chain", false)));
    json_set(kwargs, "media_root", json_string(json_get_str(in, "media_root", "")));
    json_set(args, "kwargs", kwargs);
    json_t *removed = ss_seam(io, "delete_session", args, raise_out);
    ss_refusals_exit(io, sid, *raise_out);
    if (*raise_out) { json_free(removed); return NULL; }
    json_t *ret = ss_removed(removed, raise_out);
    json_free(removed);
    return ret;
}

/* PoP: ss_delete_if_empty @ persistence/sqlite/conversation/sessions.py:delete_if_empty */
static json_t *ss_delete_if_empty(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const char *sid = json_get_str(in, "session_id", "");
    ss_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *args = json_object();
    json_t *pos = json_array();
    json_append(pos, json_string(sid));
    json_set(args, "args", pos);
    json_t *kwargs = json_object();
    json_set(kwargs, "media_root", json_string(json_get_str(in, "media_root", "")));
    json_set(args, "kwargs", kwargs);
    json_t *removed = ss_seam(io, "delete_session_if_empty", args, raise_out);
    ss_refusals_exit(io, sid, *raise_out);
    if (*raise_out) { json_free(removed); return NULL; }
    json_t *ret = ss_ids(removed, raise_out);
    json_free(removed);
    return ret;
}

/* PoP: ss_backfill_session_owners @ persistence/sqlite/conversation/sessions.py:backfill_session_owners */
static json_t *ss_backfill_session_owners(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const json_t *stamp = json_obj_get(in, "stamp");
    json_t *owner = NULL;
    if (ss_is_null(stamp)) {
        json_t *args = json_object();
        json_set(args, "args", json_array());
        json_set(args, "kwargs", json_object());
        owner = ss_seam(io, "_own_profile_name", args, raise_out);
        if (*raise_out) return NULL;
    } else {
        owner = json_copy(stamp);
    }
    if (!ss_truth(owner)) {
        json_free(owner);
        return json_number(0); /* no refusals block on the empty-owner path */
    }
    ss_refusals_enter(io, NULL, raise_out);
    if (*raise_out) { json_free(owner); return NULL; }
    json_t *args = json_object();
    json_t *pos = json_array();
    json_append(pos, json_copy(owner));
    json_set(args, "args", pos);
    json_set(args, "kwargs", json_object());
    json_t *r = ss_seam(io, "backfill_null_session_profiles", args, raise_out);
    ss_refusals_exit(io, NULL, *raise_out);
    json_free(owner);
    if (*raise_out) { json_free(r); return NULL; }
    double d = ss_int_or_0(r, raise_out);
    json_free(r);
    if (*raise_out) return NULL;
    return json_number(d);
}

/* PoP: ss_get_title_source @ persistence/sqlite/conversation/sessions.py:get_title_source */
static json_t *ss_get_title_source(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const char *sid = json_get_str(in, "session_id", "");
    ss_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *args = json_object();
    json_t *pos = json_array();
    json_append(pos, json_string(sid));
    json_set(args, "args", pos);
    json_set(args, "kwargs", json_object());
    json_t *r = ss_seam(io, "get_session_title_source", args, raise_out);
    ss_refusals_exit(io, sid, *raise_out);
    if (*raise_out) { json_free(r); return NULL; }
    const char *mapped = ss_title_source_map(r);
    json_t *ret = mapped ? json_string(mapped) : NULL;
    json_free(r);
    return ret;
}

/* PoP: ss_title_holder @ persistence/sqlite/conversation/sessions.py:title_holder */
static json_t *ss_title_holder(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const json_t *title = json_obj_get(in, "title");
    if (!ss_truth(title)) return NULL; /* before any refusals/read event */
    ss_refusals_enter(io, NULL, raise_out);
    if (*raise_out) return NULL;
    json_t *args = json_object();
    json_set(args, "sql", json_string("SELECT id FROM sessions WHERE title = ? AND id != ?"));
    json_t *params = json_array();
    json_append(params, json_copy(title));
    json_append(params, json_string(json_get_str(in, "excluding", "")));
    json_set(args, "params", params);
    json_t *row = ss_seam(io, "conn.execute", args, raise_out);
    ss_refusals_exit(io, NULL, *raise_out);
    if (*raise_out) { json_free(row); return NULL; }
    json_t *ret = NULL;
    if (ss_truth(row) && row->type == JSON_ARRAY && row->c.count > 0)
        ret = json_copy(row->c.items[0]);
    json_free(row);
    return ret;
}

/* PoP: ss_list_recent_sessions @ persistence/sqlite/conversation/sessions.py:list_recent_sessions */
static json_t *ss_list_recent_sessions(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const json_t *query = json_obj_get(in, "query");
    if (query && query->type != JSON_OBJECT) query = NULL;
    static const char *QFIELDS[] = {
        "limit", "exclude_sources", "timeout_seconds", "candidate_limit", "lineage_limit",
    };
    if (query) {
        for (size_t i = 0; i < query->c.count; i++) {
            const char *k = query->c.keys[i];
            bool known = false;
            for (size_t j = 0; j < 5 && !known; j++) known = strcmp(k, QFIELDS[j]) == 0;
            if (!known) {
                ss_raise_fmt(raise_out, "TypeError",
                             "RecentBrowseQuery.__init__() got an unexpected keyword argument '%s'", k);
                return NULL;
            }
        }
    }
    /* RecentBrowseQuery.__post_init__ clamping. */
    const json_t *limit_n = query ? json_obj_get(query, "limit") : NULL;
    json_t *limit_def = limit_n ? NULL : json_number(20);
    double limit = ss_py_int(limit_n ? limit_n : limit_def, raise_out);
    json_free(limit_def);
    if (*raise_out) return NULL;
    limit = fmax(1, limit);
    const json_t *cand_n = query ? json_obj_get(query, "candidate_limit") : NULL;
    double candidate;
    if (!ss_is_null(cand_n)) {
        candidate = ss_py_int(cand_n, raise_out);
        if (*raise_out) return NULL;
    } else {
        candidate = fmax(128, limit * 8);
    }
    candidate = fmax(limit, fmin(candidate, 2048));
    const json_t *lin_n = query ? json_obj_get(query, "lineage_limit") : NULL;
    double lineage;
    if (!ss_is_null(lin_n)) {
        lineage = ss_py_int(lin_n, raise_out);
        if (*raise_out) return NULL;
    } else {
        lineage = fmin(8192, candidate * 8);
    }
    lineage = fmax(candidate, fmin(lineage, 8192));
    const json_t *ts_n = query ? json_obj_get(query, "timeout_seconds") : NULL;
    json_t *ts_def = ts_n ? NULL : json_number(3.0);
    double timeout = ss_py_float(ts_n ? ts_n : ts_def, raise_out);
    json_free(ts_def);
    if (*raise_out) return NULL;
    timeout = fmax(0.0, timeout);
    const json_t *es = query ? json_obj_get(query, "exclude_sources") : NULL;
    json_t *listed = ss_listed(es);
    ss_refusals_enter(io, NULL, raise_out);
    if (*raise_out) { json_free(listed); return NULL; }
    json_t *args = json_object();
    json_set(args, "args", json_array());
    json_t *kwargs = json_object();
    json_set(kwargs, "limit", json_number(limit));
    if (listed) json_set(kwargs, "exclude_sources", listed);
    json_set(kwargs, "timeout_seconds", json_number(timeout));
    json_set(kwargs, "candidate_limit", json_number(candidate));
    json_set(kwargs, "lineage_limit", json_number(lineage));
    json_set(args, "kwargs", kwargs);
    json_t *rows = ss_seam(io, "list_recent_sessions_bounded", args, raise_out);
    /* except TimeoutError -> RecentBrowseTimeout(str(exc)); others pass. */
    if (*raise_out && strcmp(ss_kind(*raise_out), "TimeoutError") == 0) {
        json_t *converted = json_copy(*raise_out);
        json_set(converted, "__raise__", json_string("RecentBrowseTimeout"));
        json_free(*raise_out);
        *raise_out = converted;
    }
    ss_refusals_exit(io, NULL, *raise_out);
    if (*raise_out) { json_free(rows); return NULL; }
    if (!rows || rows->type != JSON_ARRAY) {
        ss_raise_fmt(raise_out, "TypeError", "'%s' object is not iterable", ss_typename(rows));
        json_free(rows);
        return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        /* kwarg order: the key checks interleave with the started_at float()
         * and the trailing _last_active float(). */
        static const char *HEAD[] = {"id", "source", "model", "title"};
        bool missing_key = false;
        for (size_t j = 0; j < 4 && !missing_key; j++) {
            if (!row || !json_has(row, HEAD[j])) {
                ss_raise(raise_out, "KeyError", HEAD[j]);
                missing_key = true;
            }
        }
        if (missing_key) break;
        if (!json_has(row, "started_at")) { ss_raise(raise_out, "KeyError", "started_at"); break; }
        double started = ss_float_or_0(json_obj_get(row, "started_at"), raise_out);
        if (*raise_out) break;
        static const char *TAIL[] = {"ended_at", "end_reason", "message_count", "tool_call_count"};
        for (size_t j = 0; j < 4 && !missing_key; j++) {
            if (!json_has(row, TAIL[j])) {
                ss_raise(raise_out, "KeyError", TAIL[j]);
                missing_key = true;
            }
        }
        if (missing_key) break;
        json_t *la = ss_last_active(row, raise_out);
        if (*raise_out) break;
        json_t *e = json_object();
        json_set(e, "session_id", json_copy(json_obj_get(row, "id")));
        const json_t *src = json_obj_get(row, "source");
        json_set(e, "source", ss_truth(src) ? json_copy(src) : json_string(""));
        json_set(e, "model", json_copy(json_obj_get(row, "model")));
        json_set(e, "title", json_copy(json_obj_get(row, "title")));
        json_set(e, "started_at", json_number(started));
        json_set(e, "ended_at", ss_cp(json_obj_get(row, "ended_at")));
        json_set(e, "end_reason", ss_cp(json_obj_get(row, "end_reason")));
        json_set(e, "message_count", json_copy(json_obj_get(row, "message_count")));
        json_set(e, "tool_call_count", json_copy(json_obj_get(row, "tool_call_count")));
        json_set(e, "last_active", la ? la : json_null());
        const json_t *preview = json_obj_get(row, "preview");
        json_set(e, "preview", ss_truth(preview) ? json_copy(preview) : json_string(""));
        json_set(e, "lineage_root_id", ss_cp(json_obj_get(row, "_lineage_root_id")));
        json_set(e, "unread", json_bool(ss_truth(json_obj_get(row, "unread"))));
        json_append(out, e);
    }
    json_free(rows);
    if (*raise_out) { json_free(out); return NULL; }
    return out;
}

/* PoP: ss_list_unlinked_telegram_sessions @ persistence/sqlite/conversation/sessions.py:list_unlinked_telegram_sessions */
static json_t *ss_list_unlinked_telegram_sessions(const json_t *in, ss_io_t *io, json_t **raise_out) {
    const json_t *pn = json_obj_get(in, "profile_name");
    json_t *profile = NULL;
    if (ss_is_null(pn)) {
        json_t *args = json_object();
        json_set(args, "args", json_array());
        json_set(args, "kwargs", json_object());
        json_t *r = ss_seam(io, "_own_profile_name", args, raise_out);
        if (*raise_out) return NULL;
        profile = ss_truth(r) ? r : json_string("default");
        if (profile != r) json_free(r);
    } else {
        profile = json_copy(pn);
    }
    ss_refusals_enter(io, NULL, raise_out);
    if (*raise_out) { json_free(profile); return NULL; }
    json_t *args = json_object();
    json_set(args, "args", json_array());
    json_t *kwargs = json_object();
    json_set(kwargs, "chat_id", json_string(""));
    json_set(kwargs, "user_id", json_string(json_get_str(in, "user_id", "")));
    json_set(kwargs, "profile_name", json_copy(profile));
    json_set(kwargs, "limit", json_number(json_get_num(in, "limit", 10)));
    json_set(args, "kwargs", kwargs);
    json_t *rows = ss_seam(io, "list_unlinked_telegram_sessions_for_user", args, raise_out);
    ss_refusals_exit(io, NULL, *raise_out);
    json_free(profile);
    if (*raise_out) { json_free(rows); return NULL; }
    if (!rows || rows->type != JSON_ARRAY) {
        ss_raise_fmt(raise_out, "TypeError", "'%s' object is not iterable", ss_typename(rows));
        json_free(rows);
        return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        json_t *session = ss_session_record(row, io, raise_out);
        if (*raise_out) break;
        json_t *la = ss_last_active(row, raise_out);
        if (*raise_out) { json_free(session); break; }
        json_t *e = json_object();
        json_set(e, "session", session);
        json_set(e, "last_active", la ? la : json_null());
        const json_t *preview = json_obj_get(row, "preview");
        json_set(e, "preview", ss_truth(preview) ? json_copy(preview) : json_string(""));
        json_append(out, e);
    }
    json_free(rows);
    if (*raise_out) { json_free(out); return NULL; }
    return out;
}

/* ── dispatch ── */

static json_t *ss_dispatch(const char *func, const json_t *in, ss_io_t *io, json_t **raise_out) {
    if (strcmp(func, "_stripped") == 0)
        return ss_stripped(json_obj_get(in, "value"));
    if (strcmp(func, "_json_value") == 0)
        return ss_json_value(json_obj_get(in, "text"));
    if (strcmp(func, "_tool_pin") == 0)
        return ss_tool_pin(json_obj_get(in, "text"));
    if (strcmp(func, "_ids") == 0)
        return ss_ids(json_obj_get(in, "values"), raise_out);
    if (strcmp(func, "_removed") == 0)
        return ss_removed(json_obj_get(in, "values"), raise_out);
    if (strcmp(func, "_listed") == 0)
        return ss_listed(json_obj_get(in, "values"));
    if (strcmp(func, "_optional_list") == 0)
        return ss_optional_list(json_obj_get(in, "values"));
    if (strcmp(func, "_sql_limit") == 0)
        return ss_sql_limit(json_obj_get(in, "window"), raise_out);
    if (strcmp(func, "_last_active") == 0)
        return ss_last_active(json_obj_get(in, "row"), raise_out);
    if (strcmp(func, "_roster_session") == 0)
        return ss_roster_session(json_obj_get(in, "fields"), raise_out);
    if (strcmp(func, "session_record") == 0) {
        /* drive passes inp.get("row") or {}: null/absent reads as {} */
        const json_t *row = json_obj_get(in, "row");
        json_t *empty = ss_is_null(row) ? json_object() : NULL;
        json_t *ret = ss_session_record(empty ? empty : row, io, raise_out);
        json_free(empty);
        return ret;
    }
    if (strcmp(func, "_list_entry") == 0) {
        const json_t *row = json_obj_get(in, "row");
        json_t *empty = ss_is_null(row) ? json_object() : NULL;
        json_t *ret = ss_list_entry(empty ? empty : row, json_get_bool(in, "compact", false), io, raise_out);
        json_free(empty);
        return ret;
    }
    if (strcmp(func, "_recency_entry") == 0) {
        const json_t *row = json_obj_get(in, "row");
        json_t *empty = ss_is_null(row) ? json_object() : NULL;
        json_t *ret = ss_recency_entry(empty ? empty : row, io, raise_out);
        json_free(empty);
        return ret;
    }
    if (strcmp(func, "_title_refusals") == 0)
        return ss_title_refusals(json_obj_get(in, "body"), raise_out);
    if (strcmp(func, "create") == 0)
        return ss_create(in, io, raise_out);
    if (strcmp(func, "set_title") == 0)
        return ss_set_title(in, io, raise_out);
    if (strcmp(func, "set_auto_title") == 0)
        return ss_set_auto_title(in, io, raise_out);
    if (strcmp(func, "delete") == 0)
        return ss_delete(in, io, raise_out);
    if (strcmp(func, "delete_if_empty") == 0)
        return ss_delete_if_empty(in, io, raise_out);
    if (strcmp(func, "backfill_session_owners") == 0)
        return ss_backfill_session_owners(in, io, raise_out);
    if (strcmp(func, "get_title_source") == 0)
        return ss_get_title_source(in, io, raise_out);
    if (strcmp(func, "title_holder") == 0)
        return ss_title_holder(in, io, raise_out);
    if (strcmp(func, "list_recent_sessions") == 0)
        return ss_list_recent_sessions(in, io, raise_out);
    if (strcmp(func, "list_unlinked_telegram_sessions") == 0)
        return ss_list_unlinked_telegram_sessions(in, io, raise_out);
    ss_raise_fmt(raise_out, "ValueError", "unknown func: %s", func);
    return NULL;
}