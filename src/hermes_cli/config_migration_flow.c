/* config_migration_flow.c — FLOW + FUNCTION port of the config migration /
 * backend-selection flows in hermes_cli/left_core_migration.py and
 * hermes_cli/config_backend.py.
 *
 * Ports the DECISION SEQUENCES of the left-core migration pipeline:
 *   the installed/used/scoped marker predicates (_marked, _mark), the
 *   core-era toolset scope predicate (_core_carried), the left_at version
 *   gate (_predates_split, record_core_use), the one-time toolset scope
 *   conversion (_turn_off_replaced, _keep_legacy_disable, _record_migration
 *   with its created -> _stamp_created -> atomic_config_write ordering),
 *   the install decision chain (_install_one: install exception -> result,
 *   ok -> refresh_bound_home -> _record_migration -> say; failure ->
 *   _note_failure -> _first_cause -> say) and the top-level per-home
 *   ordering (migrate_home).
 * And the config backend surface:
 *   the version token (version_token: int-led /1e9 vs crc32(repr(tuple))),
 *   backend selection (_backend_for: file / kind regex / sys.modules /
 *   import ImportError name routing / get_backend callable;
 *   get_config_backend: bootstrap -> env strip/lower/or-file),
 *   the once-per-process bootstrap state machine (_bootstrap,
 *   _ensure_bootstrap_env: owner re-entry, ImportError swallow, finally
 *   publication), the user-layer vs explicit-file routing (_route),
 *   the read path (read_config_doc), the file write ordering
 *   (FileBackend.write_path: document -> set -> unset -> drop_other_access),
 *   the OSError-tolerant cache signature (config_cache_version_or_none)
 *   and the grouped key write (write_config_keys: redacted-set refusal ->
 *   route -> None-split set/unset -> one write).
 *
 * All file / env / backend / catalog I/O is INJECTED through the cmf_io_t
 * seam dispatcher (model mirrors config_load_save_flow.c): every I/O call
 * goes through cmf_seam(io, name, args), which appends {"name", "args"} to
 * io->trace, dispatches, and frees args. The harness dispatcher returns the
 * scripted result, applies "state" effects to io->state, and models a raised
 * exception with {"__raise__": {type,message,...}}; an "import_raise" entry
 * models a failed in-function import ({"__untrace__": true} retracts the
 * trace event — the live import statement records nothing before raising).
 *
 * io->state mirrors the config_backend module globals the flows mutate:
 *   _BOOTSTRAPPED (bool), _BOOTSTRAP_OWNER (int or null).
 *
 * Representation contract (both sides):
 * - paths reach traces/results as posix strings (Path.as_posix).
 * - version_token input elements carry explicit type tags ({"int":n},
 *   {"float":x}, {"str":s}, {"bool":b}, {"null":true}, {"tuple":[...]})
 *   because a parsed JSON number has no int/float identity and the live
 *   isinstance(version[0], int) branch depends on it.
 * - backends ride as descriptor objects: {"name":"file"} for the file
 *   backend, {"explicit":<posix path>} for _ExplicitFile, or the scripted
 *   remote descriptor.
 * - an uncaught exception rides as {"__raise__": {type,message,code?,name?}}.
 *   ConfigBackendUnavailable IS a SystemExit (code == message).
 * - str() of a JSON number formats integral doubles as integers (%.0f);
 *   a literal 1.0 reads back as 1 (documented libjson identity exception —
 *   observable only where a numeric str() reaches a result, which the
 *   fixture avoids; marker membership compares against alphabetic plugin /
 *   toolset names, where the distinction cannot matter).
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * are harness-owned. cmf_seam() frees its args. Every public cmf_* function
 * returns a NEW reference (or a propagated raise sentinel). Mutated input
 * dicts (config) are borrowed and mutated in place.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include "hermes_json.h"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct cmf_io_t {
    json_t *(*call)(struct cmf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ARRAY of {"name","args"} events (harness-owned) */
    json_t *state;   /* OBJECT: _BOOTSTRAPPED / _BOOTSTRAP_OWNER (harness-owned) */
    void   *ctx;
} cmf_io_t;

static bool cmf_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *cmf_raise_spec(const json_t *r) {
    if (!cmf_raised(r)) return NULL;
    return json_obj_get(r, "__raise__");
}

static json_t *cmf_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

/* ConfigBackendUnavailable / SystemExit: code == the message string. */
static json_t *cmf_make_raise_code_str(const char *type, const char *message) {
    json_t *r = cmf_make_raise(type, message);
    json_set(cmf_raise_spec(r), "code", json_string(message ? message : ""));
    return r;
}

static json_t *cmf_seam(cmf_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    if (io && io->trace && io->trace->type == JSON_ARRAY)
        json_append(io->trace, ev);
    else
        json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : json_null();
    if (args) json_free(args);
    if (!r) r = json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
            size_t last = io->trace->c.count - 1;
            json_free(io->trace->c.items[last]);
            io->trace->c.count = last;
        }
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = json_object();
        json_set(out, "__raise__", spec ? json_copy(spec) : json_null());
        json_free(r);
        return out;
    }
    return r;
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool cmf_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static bool cmf_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool cmf_json_eq(const json_t *a, const json_t *b) {
    if (cmf_is_null(a)) return cmf_is_null(b);
    if (cmf_is_null(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!cmf_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !cmf_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* dynamic string buffer */
typedef struct { char *s; size_t len, cap; } cmf_buf_t;

static void cmf_binit(cmf_buf_t *b) { b->s = NULL; b->len = 0; b->cap = 0; }

static void cmf_bputn(cmf_buf_t *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *ns = (char *)realloc(b->s, nc);
        if (!ns) return;
        b->s = ns; b->cap = nc;
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}

static void cmf_bput(cmf_buf_t *b, const char *s) { if (s) cmf_bputn(b, s, strlen(s)); }
static void cmf_bputc(cmf_buf_t *b, char c) { cmf_bputn(b, &c, 1); }

/* UTF-8 decode: returns codepoint, advances *p; invalid bytes -> U+FFFD-ish
 * passthrough of the single byte (fixture strings are valid UTF-8). */
static uint32_t cmf_utf8_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    uint32_t cp = s[0];
    if (cp < 0x80) { *p += 1; return cp; }
    int extra = 0;
    if ((cp & 0xE0) == 0xC0) { cp &= 0x1F; extra = 1; }
    else if ((cp & 0xF0) == 0xE0) { cp &= 0x0F; extra = 2; }
    else if ((cp & 0xF8) == 0xF0) { cp &= 0x07; extra = 3; }
    else { *p += 1; return cp; }
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return s[0]; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p += extra + 1;
    return cp;
}

/* Python float repr: shortest round-trip digits, decimal for -4 <= X < 16,
 * scientific otherwise (e+XX, >= 2 exponent digits), always a '.' or 'e'. */
static char *cmf_repr_float(double v) {
    cmf_buf_t b; cmf_binit(&b);
    if (isnan(v)) { cmf_bput(&b, "nan"); return b.s; }
    if (isinf(v)) { cmf_bput(&b, v < 0 ? "-inf" : "inf"); return b.s; }
    if (v == 0.0) { cmf_bput(&b, signbit(v) ? "-0.0" : "0.0"); return b.s; }
    bool neg = signbit(v);
    double a = fabs(v);
    char buf[64];
    int p;
    for (p = 1; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*e", p - 1, a);
        if (strtod(buf, NULL) == a) break;
    }
    if (p > 17) p = 17;
    /* parse d.ddddde+XX */
    char digits[32];
    size_t nd = 0;
    const char *q = buf;
    while (*q && *q != 'e' && *q != 'E') {
        if (*q >= '0' && *q <= '9') digits[nd++] = *q;
        q++;
    }
    digits[nd] = '\0';
    int X = (*q) ? atoi(q + 1) : 0;
    if (neg) cmf_bputc(&b, '-');
    if (X >= 16 || X < -4) {
        cmf_bputc(&b, digits[0]);
        if (nd > 1) { cmf_bputc(&b, '.'); cmf_bput(&b, digits + 1); }
        cmf_bputc(&b, 'e');
        cmf_bputc(&b, X < 0 ? '-' : '+');
        char eb[16];
        snprintf(eb, sizeof eb, "%02d", X < 0 ? -X : X);
        cmf_bput(&b, eb);
    } else {
        int id = X + 1;
        if (id <= 0) {
            cmf_bput(&b, "0.");
            for (int i = 0; i < -id; i++) cmf_bputc(&b, '0');
            cmf_bput(&b, digits);
        } else if ((size_t)id >= nd) {
            cmf_bput(&b, digits);
            for (size_t i = nd; i < (size_t)id; i++) cmf_bputc(&b, '0');
            cmf_bput(&b, ".0");
        } else {
            cmf_bputn(&b, digits, (size_t)id);
            cmf_bputc(&b, '.');
            cmf_bput(&b, digits + id);
        }
    }
    return b.s;
}

/* Python str repr: single quotes unless the string has ' and no ";
 * escapes \\, the quote, \n \r \t, C0/C1 controls, NBSP, soft hyphen,
 * U+2028/U+2029; printable non-ASCII stays raw UTF-8. */
static char *cmf_repr_str(const char *s) {
    cmf_buf_t b; cmf_binit(&b);
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    cmf_bputc(&b, quote);
    const char *p = s;
    while (*p) {
        const char *start = p;
        uint32_t cp = cmf_utf8_next(&p);
        if (cp == (uint32_t)quote || cp == '\\') {
            cmf_bputc(&b, '\\');
            cmf_bputc(&b, (char)cp);
        } else if (cp == '\n') cmf_bput(&b, "\\n");
        else if (cp == '\r') cmf_bput(&b, "\\r");
        else if (cp == '\t') cmf_bput(&b, "\\t");
        else if (cp < 0x20 || (cp >= 0x7f && cp <= 0x9f) || cp == 0xa0 ||
                 cp == 0xad || cp == 0x2028 || cp == 0x2029) {
            char eb[16];
            if (cp < 0x100) snprintf(eb, sizeof eb, "\\x%02x", (unsigned)cp);
            else if (cp < 0x10000) snprintf(eb, sizeof eb, "\\u%04x", (unsigned)cp);
            else snprintf(eb, sizeof eb, "\\U%08x", (unsigned)cp);
            cmf_bput(&b, eb);
        } else {
            cmf_bputn(&b, start, (size_t)(p - start));
        }
    }
    cmf_bputc(&b, quote);
    return b.s;
}

static char *cmf_py_repr(const json_t *v);

static char *cmf_py_repr(const json_t *v) {
    cmf_buf_t b; cmf_binit(&b);
    if (!v || v->type == JSON_NULL) { cmf_bput(&b, "None"); return b.s; }
    switch (v->type) {
        case JSON_BOOL:
            cmf_bput(&b, v->bool_val ? "True" : "False");
            return b.s;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == trunc(d) && fabs(d) < 1e18) {
                char nb[40];
                snprintf(nb, sizeof nb, "%.0f", d);
                cmf_bput(&b, nb);
            } else {
                char *fs = cmf_repr_float(d);
                cmf_bput(&b, fs);
                free(fs);
            }
            return b.s;
        }
        case JSON_STRING: {
            char *rs = cmf_repr_str(v->str_val ? v->str_val : "");
            cmf_bput(&b, rs);
            free(rs);
            return b.s;
        }
        case JSON_ARRAY: {
            cmf_bputc(&b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) cmf_bput(&b, ", ");
                char *rs = cmf_py_repr(v->c.items[i]);
                cmf_bput(&b, rs);
                free(rs);
            }
            cmf_bputc(&b, ']');
            return b.s;
        }
        case JSON_OBJECT: {
            cmf_bputc(&b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) cmf_bput(&b, ", ");
                char *ks = cmf_repr_str(v->c.keys[i] ? v->c.keys[i] : "");
                cmf_bput(&b, ks);
                free(ks);
                cmf_bput(&b, ": ");
                char *rs = cmf_py_repr(v->c.items[i]);
                cmf_bput(&b, rs);
                free(rs);
            }
            cmf_bputc(&b, '}');
            return b.s;
        }
        default:
            cmf_bput(&b, "None");
            return b.s;
    }
}

/* str(v): strings verbatim, everything else repr(v). */
static char *cmf_py_str(const json_t *v) {
    if (v && v->type == JSON_STRING)
        return strdup(v->str_val ? v->str_val : "");
    return cmf_py_repr(v);
}

static const char *cmf_py_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* zlib.crc32 (polynomial 0xEDB88320, init/final 0xFFFFFFFF). */
static uint32_t cmf_crc32(const char *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (unsigned char)data[i];
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

/* str.strip() over the ASCII whitespace set (+ C0 separators 1c-1f). */
static bool cmf_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f' || c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}

static char *cmf_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && cmf_is_ws(s[a])) a++;
    while (b > a && cmf_is_ws(s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *cmf_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

/* str.splitlines(): \n \r \r\n \v \f \x1c \x1d \x1e \x85   .
 * A trailing boundary does not add an empty final element. */
static char **cmf_splitlines(const char *s, size_t *n_out) {
    char **lines = NULL;
    size_t n = 0, cap = 0;
    const char *p = s ? s : "";
    const char *start = p;
    while (*p) {
        const char *before = p;
        uint32_t cp = cmf_utf8_next(&p);
        bool boundary = false;
        if (cp == '\n' || cp == '\v' || cp == '\f' || cp == 0x1c ||
            cp == 0x1d || cp == 0x1e || cp == 0x85 || cp == 0x2028 || cp == 0x2029)
            boundary = true;
        else if (cp == '\r') {
            boundary = true;
            if (*p == '\n') p++;
        }
        if (boundary) {
            if (n == cap) {
                cap = cap ? cap * 2 : 8;
                char **nl = (char **)realloc(lines, cap * sizeof(char *));
                if (!nl) break;
                lines = nl;
            }
            size_t len = (size_t)(before - start);
            lines[n] = (char *)malloc(len + 1);
            if (lines[n]) { memcpy(lines[n], start, len); lines[n][len] = '\0'; }
            n++;
            start = p;
        }
    }
    if (p > start) {
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            char **nl = (char **)realloc(lines, cap * sizeof(char *));
            if (nl) lines = nl;
        }
        if (n < cap) {
            size_t len = (size_t)(p - start);
            lines[n] = (char *)malloc(len + 1);
            if (lines[n]) { memcpy(lines[n], start, len); lines[n][len] = '\0'; }
            n++;
        }
    }
    *n_out = n;
    return lines;
}

/* int(v) for the _predates_split gate: 0 ok, 1 ValueError, 2 TypeError.
 * Strings: strip, optional sign, digits with single underscores BETWEEN
 * digits. Numbers truncate toward zero; bool is an int subclass. */
static int cmf_py_int(const json_t *v, double *out) {
    *out = 0.0;
    if (!v || v->type == JSON_NULL) return 2;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type == JSON_NUMBER) {
        if (isnan(v->num_val)) return 1;
        *out = trunc(v->num_val);
        return 0;
    }
    if (v->type != JSON_STRING) return 2;
    char *s = cmf_strip(v->str_val ? v->str_val : "");
    if (!s) return 2;
    const char *p = s;
    double sign = 1.0;
    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1.0; p++; }
    if (!(*p >= '0' && *p <= '9')) { free(s); return 1; }
    double acc = 0.0;
    bool prev_digit = false;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (acc < 1e18) acc = acc * 10.0 + (*p - '0');
            prev_digit = true;
        } else if (*p == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { free(s); return 1; }
            prev_digit = false;
        } else {
            free(s);
            return 1;
        }
        p++;
    }
    free(s);
    if (!prev_digit) return 1;   /* trailing underscore */
    *out = sign * acc;
    return 0;
}

/* ── pathlib semantics (Windows flavour, posix output) ─────────────────── */
typedef struct {
    char drive[3];      /* "C:" or "" */
    bool rooted;
    char **parts;
    size_t n;
} cmf_path_t;

static void cmf_path_parse(const char *s, cmf_path_t *p) {
    memset(p, 0, sizeof *p);
    if (!s) s = "";
    const char *rest = s;
    if (((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) && s[1] == ':') {
        p->drive[0] = s[0]; p->drive[1] = ':'; p->drive[2] = '\0';
        rest = s + 2;
    }
    if (*rest == '/' || *rest == '\\') {
        p->rooted = true;
        while (*rest == '/' || *rest == '\\') rest++;
    }
    size_t cap = 0;
    const char *seg = rest;
    while (1) {
        if (*rest == '/' || *rest == '\\' || *rest == '\0') {
            size_t len = (size_t)(rest - seg);
            if (len > 0 && !(len == 1 && seg[0] == '.')) {
                if (p->n == cap) {
                    cap = cap ? cap * 2 : 8;
                    char **np = (char **)realloc(p->parts, cap * sizeof(char *));
                    if (!np) break;
                    p->parts = np;
                }
                p->parts[p->n] = (char *)malloc(len + 1);
                if (p->parts[p->n]) {
                    memcpy(p->parts[p->n], seg, len);
                    p->parts[p->n][len] = '\0';
                }
                p->n++;
            }
            if (*rest == '\0') break;
            seg = rest + 1;
        }
        rest++;
    }
}

static void cmf_path_free(cmf_path_t *p) {
    for (size_t i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts);
    p->parts = NULL; p->n = 0;
}

/* reconstruct as_posix over the first n parts */
static char *cmf_path_build(const cmf_path_t *p, size_t n) {
    cmf_buf_t b; cmf_binit(&b);
    if (p->drive[0]) {
        cmf_bput(&b, p->drive);
        if (p->rooted) cmf_bputc(&b, '/');
    } else if (p->rooted) {
        cmf_bputc(&b, '/');
    }
    for (size_t i = 0; i < n; i++) {
        if (i) cmf_bputc(&b, '/');
        cmf_bput(&b, p->parts[i]);
    }
    if (b.len == 0) cmf_bput(&b, ".");
    return b.s;
}

static char *cmf_path_posix(const char *s) {
    cmf_path_t p;
    cmf_path_parse(s, &p);
    char *out = cmf_path_build(&p, p.n);
    cmf_path_free(&p);
    return out;
}

static char *cmf_path_name(const char *s) {
    cmf_path_t p;
    cmf_path_parse(s, &p);
    char *out = strdup(p.n ? p.parts[p.n - 1] : "");
    cmf_path_free(&p);
    return out;
}

static char *cmf_path_parent(const char *s) {
    cmf_path_t p;
    cmf_path_parse(s, &p);
    char *out = cmf_path_build(&p, p.n ? p.n - 1 : 0);
    cmf_path_free(&p);
    return out;
}

/* Path(home) / "config.yaml" */
static char *cmf_path_join(const char *home, const char *leaf) {
    cmf_path_t p;
    cmf_path_parse(home, &p);
    cmf_buf_t b; cmf_binit(&b);
    char *base = cmf_path_build(&p, p.n);
    cmf_bput(&b, base);
    free(base);
    /* base "." means empty relative path: the leaf replaces it */
    if (b.len == 1 && b.s[0] == '.') {
        b.len = 0; b.s[0] = '\0';
    } else if (b.len > 0 && b.s[b.len - 1] != '/') {
        cmf_bputc(&b, '/');
    }
    cmf_bput(&b, leaf);
    cmf_path_free(&p);
    return b.s;
}

/* sorted(set(strings)) -> JSON array of strings (byte order == codepoint
 * order for valid UTF-8). Inputs are borrowed char* list, dedup + sort. */
static int cmf_str_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *cmf_sorted_unique(char **strs, size_t n) {
    if (n > 1) qsort(strs, n, sizeof(char *), cmf_str_cmp);
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) {
        if (i > 0 && strcmp(strs[i], strs[i - 1]) == 0) continue;
        json_append(arr, json_string(strs[i]));
    }
    return arr;
}

/* str membership in a JSON list: only string elements can equal a str. */
static bool cmf_str_member(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val && strcmp(it->str_val, s) == 0)
            return true;
    }
    return false;
}

static bool cmf_cstr_in(const char *const *list, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(list[i], s) == 0) return true;
    return false;
}

/* state accessors */
static bool cmf_state_bootstrapped(cmf_io_t *io) {
    if (!io || !io->state) return false;
    json_t *v = json_obj_get(io->state, "_BOOTSTRAPPED");
    return v && v->type == JSON_BOOL && v->bool_val;
}

static json_t *cmf_state_owner(cmf_io_t *io) {
    if (!io || !io->state) return NULL;
    return json_obj_get(io->state, "_BOOTSTRAP_OWNER");
}/* ── LEFT_CORE table (byte-pinned to hermes_cli/left_core_migration.py) ──
 * Only the fields the ported flows read. Row order is the live tuple order:
 * homeassistant, spotify, langfuse, mcp-serve, camofox, singularity,
 * agent-importers, github-copilot, yuanbao.
 * _YUANBAO_OFF_PLATFORMS = _core_platforms_except("yuanbao", "homeassistant"):
 * sorted({PLATFORMS keys} | {bundled plugins/platforms dirs with plugin.yaml}
 * | {"msgraph_webhook","relay","acp","homeassistant"} - {"yuanbao"}). */
static const char *const CMF_OFF_HOMEASSISTANT[] = { "acp", "webhook" };
static const char *const CMF_OFF_CAMOFOX[] = { "webhook" };
static const char *const CMF_OFF_YUANBAO[] = {
    "a2a", "acp", "api_server", "bluebubbles", "buzz", "cli", "cron",
    "dingtalk", "discord", "email", "feishu", "google_chat", "homeassistant",
    "irc", "line", "matrix", "mattermost", "msgraph_webhook", "ntfy", "photon",
    "qqbot", "raft", "relay", "signal", "simplex", "slack", "sms", "teams",
    "telegram", "webhook", "wecom", "wecom_callback", "weixin", "whatsapp",
    "whatsapp_cloud",
};
static const char *const CMF_TS_HOMEASSISTANT[] = { "homeassistant" };
static const char *const CMF_TS_CAMOFOX[] = { "camofox" };
static const char *const CMF_TS_YUANBAO[] = { "yuanbao" };
static const char *const CMF_REPLACES_CAMOFOX[] = { "browser" };
static const char *const CMF_LEGACY_YUANBAO[] = { "hermes-yuanbao", "hermes-gateway" };

typedef struct {
    const char *plugin;
    const char *label;
    const char *unchanged;
    const char *const *toolsets;   size_t n_toolsets;
    const char *const *off;        size_t n_off;
    const char *const *replaces;   size_t n_replaces;
    const char *const *legacy;     size_t n_legacy;
    int left_at;
} cmf_feature_t;

#define CMF_FEATURE(p, l, u, ts, nts, off, noff, rp, nrp, lg, nlg, la) \
    { p, l, u, ts, nts, off, noff, rp, nrp, lg, nlg, la }

static const cmf_feature_t CMF_LEFT_CORE[] = {
    CMF_FEATURE("homeassistant", "Home Assistant",
        "HASS_TOKEN/HASS_URL, platforms.homeassistant and the ha_* tool names are unchanged",
        CMF_TS_HOMEASSISTANT, 1, CMF_OFF_HOMEASSISTANT, 2, NULL, 0, NULL, 0, 0),
    CMF_FEATURE("spotify", "Spotify",
        "your Spotify login, the spotify toolset and the spotify_* tools are unchanged; "
        "`hermes auth spotify` is now `hermes spotify login`",
        NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0),
    CMF_FEATURE("langfuse", "Langfuse",
        "your Langfuse keys and HERMES_LANGFUSE_* settings are unchanged; the plugin is now "
        "`langfuse` (was observability/langfuse)",
        NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0),
    CMF_FEATURE("mcp-serve", "Hermes as an MCP server",
        "the conversation tools keep their names; MCP clients change their args to [\"mcp-serve\"]",
        NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0),
    CMF_FEATURE("camofox", "Camofox browser backend",
        "CAMOFOX_URL, the browser.camofox settings and the browser_navigate/browser_click/... tool "
        "names are unchanged; they are the `camofox` toolset now, and core's `browser` toolset "
        "(browser_exec) is turned off in this profile",
        CMF_TS_CAMOFOX, 1, CMF_OFF_CAMOFOX, 1, CMF_REPLACES_CAMOFOX, 1, NULL, 0, 52),
    CMF_FEATURE("singularity", "Singularity/Apptainer terminal backend",
        "terminal.backend: singularity, terminal.singularity_image (TERMINAL_SINGULARITY_IMAGE) "
        "and the container_* limits are unchanged",
        NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0),
    CMF_FEATURE("agent-importers", "`hermes import-agent`",
        "`hermes import-agent`, its flags and your import-sync.json sync sources are unchanged",
        NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0),
    CMF_FEATURE("github-copilot", "GitHub Copilot",
        "your COPILOT_GITHUB_TOKEN / GH_TOKEN / GITHUB_TOKEN and `hermes auth add copilot` logins, "
        "model names and copilot-acp settings are unchanged",
        NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0),
    CMF_FEATURE("yuanbao", "Yuanbao",
        "YUANBAO_* settings, platforms.yuanbao, YUANBAO_HOME_CHANNEL and the yb_* tool names are unchanged",
        CMF_TS_YUANBAO, 1, CMF_OFF_YUANBAO, 35, NULL, 0, CMF_LEGACY_YUANBAO, 2, 0),
};
#define CMF_N_FEATURES (sizeof(CMF_LEFT_CORE) / sizeof(CMF_LEFT_CORE[0]))

static const cmf_feature_t *cmf_feature_lookup(const char *plugin) {
    if (!plugin) return NULL;
    for (size_t i = 0; i < CMF_N_FEATURES; i++)
        if (strcmp(CMF_LEFT_CORE[i].plugin, plugin) == 0)
            return &CMF_LEFT_CORE[i];
    return NULL;
}

/* except Exception catches Exception subclasses, NOT SystemExit /
 * KeyboardInterrupt / GeneratorExit (BaseException-only). */
static bool cmf_exc_is_exception(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "");
    return strcmp(t, "SystemExit") != 0 && strcmp(t, "KeyboardInterrupt") != 0 &&
           strcmp(t, "GeneratorExit") != 0;
}

static bool cmf_exc_is_oserror(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "");
    static const char *const family[] = {
        "OSError", "PermissionError", "FileNotFoundError", "FileExistsError",
        "BlockingIOError", "ChildProcessError", "ConnectionError", "BrokenPipeError",
        "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "ProcessLookupError", "TimeoutError",
    };
    for (size_t i = 0; i < sizeof(family) / sizeof(family[0]); i++)
        if (strcmp(t, family[i]) == 0) return true;
    return false;
}

/* ── _marked / _mark / _scope_kept ─────────────────────────────────────── */

/* PoP: cmf_marked @ hermes_cli/left_core_migration.py:_marked */
bool cmf_marked(const json_t *config, const char *key, const cmf_feature_t *f) {
    json_t *done = json_obj_get(config, key);
    if (!done || done->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < done->c.count; i++) {
        char *s = cmf_py_str(done->c.items[i]);
        bool hit = s && strcmp(s, f->plugin) == 0;
        free(s);
        if (hit) return true;
    }
    return false;
}

/* _mark: config[key] = sorted({*map(str, done) if list, plugin}).
 * Untagged helper: exercised through record_core_use / _record_migration. */
static void cmf_mark(json_t *config, const char *key, const cmf_feature_t *f) {
    char **strs = NULL;
    size_t n = 0, cap = 0;
    json_t *done = json_obj_get(config, key);
    if (done && done->type == JSON_ARRAY) {
        for (size_t i = 0; i < done->c.count; i++) {
            if (n == cap) { cap = cap ? cap * 2 : 8; strs = (char **)realloc(strs, cap * sizeof(char *)); }
            strs[n++] = cmf_py_str(done->c.items[i]);
        }
    }
    if (n == cap) { cap = cap ? cap * 2 : 8; strs = (char **)realloc(strs, cap * sizeof(char *)); }
    strs[n++] = strdup(f->plugin);
    json_t *arr = cmf_sorted_unique(strs, n);
    for (size_t i = 0; i < n; i++) free(strs[i]);
    free(strs);
    json_set(config, key, arr);
}

/* _scope_kept: not feature.toolsets or _marked(config, _SCOPED_KEY, feature) */
static bool cmf_scope_kept(const json_t *config, const cmf_feature_t *f) {
    return f->n_toolsets == 0 || cmf_marked(config, "_left_core_scoped", f);
}

/* ── _core_carried ─────────────────────────────────────────────────────── */
static bool cmf_carries(const cmf_feature_t *f, const char *name) {
    if (cmf_cstr_in(f->toolsets, f->n_toolsets, name) ||
        cmf_cstr_in(f->replaces, f->n_replaces, name))
        return true;
    if (strcmp(name, "hermes") == 0)
        return cmf_carries(f, "hermes-cli") || cmf_carries(f, "hermes-api-server");
    if (strncmp(name, "hermes-", 7) == 0) {
        char *suffix = strdup(name + 7);
        if (!suffix) return false;
        for (char *p = suffix; *p; p++)
            if (*p == '-') *p = '_';
        bool off = cmf_cstr_in(f->off, f->n_off, suffix);
        free(suffix);
        return !off;
    }
    return false;
}

/* PoP: cmf_core_carried @ hermes_cli/left_core_migration.py:_core_carried */
bool cmf_core_carried(const cmf_feature_t *f, const json_t *selection) {
    if (!selection || selection->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < selection->c.count; i++) {
        char *s = cmf_py_str(selection->c.items[i]);
        bool hit = s && cmf_carries(f, s);
        free(s);
        if (hit) return true;
    }
    return false;
}

/* ── _read_config (untagged helper) ────────────────────────────────────── */
static json_t *cmf_read_config(cmf_io_t *io, const char *home) {
    char *path = cmf_path_join(home, "config.yaml");
    json_t *args = json_object();
    json_set(args, "path", json_string(path));
    free(path);
    json_t *r = cmf_seam(io, "read_config_doc", args);
    if (cmf_raised(r)) {
        if (cmf_exc_is_exception(cmf_raise_spec(r))) {
            json_free(r);
            return json_object();
        }
        return r;   /* SystemExit escapes except Exception */
    }
    if (r && r->type == JSON_OBJECT) return r;
    json_free(r);
    return json_object();
}

/* ── _predates_split ───────────────────────────────────────────────────── */

/* PoP: cmf_predates_split @ hermes_cli/left_core_migration.py:_predates_split */
json_t *cmf_predates_split(cmf_io_t *io, const char *home, const cmf_feature_t *f) {
    if (!f->left_at) return json_bool(true);
    json_t *config = cmf_read_config(io, home);
    if (cmf_raised(config)) return config;
    bool early = cmf_marked(config, "_left_core_used", f) ||
                 !json_has(config, "_config_version");
    if (early) { json_free(config); return json_bool(true); }
    json_t *v = json_obj_get(config, "_config_version");
    double iv = 0.0;
    int rc = cmf_py_int(v, &iv);
    json_free(config);
    if (rc != 0) return json_bool(false);   /* (TypeError, ValueError) caught */
    return json_bool(iv < (double)f->left_at);
}

/* ── record_core_use ───────────────────────────────────────────────────── */

/* PoP: cmf_record_core_use @ hermes_cli/left_core_migration.py:record_core_use */
json_t *cmf_record_core_use(cmf_io_t *io, const char *home, json_t *config, int version) {
    const cmf_feature_t *used[CMF_N_FEATURES];
    size_t n_used = 0;
    char *home_posix = cmf_path_posix(home);
    for (size_t i = 0; i < CMF_N_FEATURES; i++) {
        const cmf_feature_t *f = &CMF_LEFT_CORE[i];
        if (f->left_at != version) continue;
        if (cmf_marked(config, "_left_core_used", f)) continue;
        json_t *args = json_object();
        json_set(args, "plugin", json_string(f->plugin));
        json_set(args, "home", json_string(home_posix));
        json_set(args, "process_env", json_bool(true));
        json_t *r = cmf_seam(io, "feature_in_use", args);
        if (cmf_raised(r)) { free(home_posix); return r; }
        if (cmf_truthy(r)) used[n_used++] = f;
        json_free(r);
    }
    for (size_t i = 0; i < n_used; i++)
        cmf_mark(config, "_left_core_used", used[i]);
    free(home_posix);
    return json_bool(n_used > 0);
}

/* ── _turn_off_replaced ────────────────────────────────────────────────── */

/* PoP: cmf_turn_off_replaced @ hermes_cli/left_core_migration.py:_turn_off_replaced */
json_t *cmf_turn_off_replaced(cmf_io_t *io, json_t *config, const cmf_feature_t *f) {
    json_t *saved = json_obj_get(config, "platform_toolsets");
    if (saved && saved->type == JSON_OBJECT) {
        for (size_t i = 0; i < saved->c.count; i++) {
            const char *platform = saved->c.keys[i];
            json_t *args = json_object();
            json_set(args, "value", json_copy(saved->c.items[i]));
            json_t *r = cmf_seam(io, "parse_platform_toolsets_value", args);
            if (cmf_raised(r)) return r;
            if (!cmf_is_null(r)) {
                if (r->type != JSON_ARRAY) {
                    char msg[128];
                    snprintf(msg, sizeof msg, "'%s' object is not iterable", cmf_py_typename(r));
                    json_free(r);
                    return cmf_make_raise("TypeError", msg);
                }
                bool intersect = false;
                for (size_t j = 0; j < r->c.count && !intersect; j++) {
                    char *s = cmf_py_str(r->c.items[j]);
                    intersect = s && cmf_cstr_in(f->replaces, f->n_replaces, s);
                    free(s);
                }
                if (intersect) {
                    json_t *kept = json_array();
                    for (size_t j = 0; j < r->c.count; j++) {
                        char *s = cmf_py_str(r->c.items[j]);
                        bool drop = s && cmf_cstr_in(f->replaces, f->n_replaces, s);
                        free(s);
                        if (!drop) json_append(kept, json_copy(r->c.items[j]));
                    }
                    for (size_t t = 0; t < f->n_toolsets; t++)
                        if (!cmf_str_member(kept, f->toolsets[t]))
                            json_append(kept, json_string(f->toolsets[t]));
                    json_set(saved, platform, kept);
                }
            }
            json_free(r);
        }
    }
    json_t *agent = json_obj_get(config, "agent");
    bool agent_existing = agent && agent->type == JSON_OBJECT;
    if (!agent_existing) agent = json_object();
    json_t *dv = json_obj_get(agent, "disabled_toolsets");
    json_t *disabled = NULL;
    if (cmf_truthy(dv)) {
        json_t *args = json_object();
        json_set(args, "value", json_copy(dv));
        json_t *r = cmf_seam(io, "parse_config_string_list", args);
        if (cmf_raised(r)) { if (!agent_existing) json_free(agent); return r; }
        if (r->type != JSON_ARRAY) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", cmf_py_typename(r));
            json_free(r);
            if (!agent_existing) json_free(agent);
            return cmf_make_raise("TypeError", msg);
        }
        disabled = r;
    } else {
        disabled = json_array();
    }
    bool disjoint = true;
    for (size_t i = 0; i < f->n_replaces && disjoint; i++)
        if (cmf_str_member(disabled, f->replaces[i])) disjoint = false;
    /* turned_off = replaced if disjoint else replaced | toolsets */
    char **extra = NULL;
    size_t n_extra = 0, cap = 0;
    for (size_t i = 0; i < f->n_replaces; i++) {
        if (cmf_str_member(disabled, f->replaces[i])) continue;
        if (n_extra == cap) { cap = cap ? cap * 2 : 8; extra = (char **)realloc(extra, cap * sizeof(char *)); }
        extra[n_extra++] = strdup(f->replaces[i]);
    }
    if (!disjoint) {
        for (size_t i = 0; i < f->n_toolsets; i++) {
            if (cmf_str_member(disabled, f->toolsets[i])) continue;
            bool dup = false;
            for (size_t j = 0; j < n_extra && !dup; j++)
                if (strcmp(extra[j], f->toolsets[i]) == 0) dup = true;
            if (dup) continue;
            if (n_extra == cap) { cap = cap ? cap * 2 : 8; extra = (char **)realloc(extra, cap * sizeof(char *)); }
            extra[n_extra++] = strdup(f->toolsets[i]);
        }
    }
    json_t *sorted_extra = cmf_sorted_unique(extra, n_extra);
    for (size_t i = 0; i < n_extra; i++) free(extra[i]);
    free(extra);
    json_t *new_disabled = json_array();
    for (size_t i = 0; i < disabled->c.count; i++)
        json_append(new_disabled, json_copy(disabled->c.items[i]));
    for (size_t i = 0; i < sorted_extra->c.count; i++)
        json_append(new_disabled, json_copy(sorted_extra->c.items[i]));
    json_free(sorted_extra);
    json_free(disabled);
    json_set(agent, "disabled_toolsets", new_disabled);
    if (!agent_existing) json_set(config, "agent", agent);
    return json_null();
}

/* ── _keep_legacy_disable ──────────────────────────────────────────────── */

/* PoP: cmf_keep_legacy_disable @ hermes_cli/left_core_migration.py:_keep_legacy_disable */
json_t *cmf_keep_legacy_disable(cmf_io_t *io, json_t *config, const cmf_feature_t *f) {
    json_t *agent = json_obj_get(config, "agent");
    bool agent_existing = agent && agent->type == JSON_OBJECT;
    if (!agent_existing) agent = json_object();
    json_t *dv = json_obj_get(agent, "disabled_toolsets");
    json_t *disabled = NULL;
    if (cmf_truthy(dv)) {
        json_t *args = json_object();
        json_set(args, "value", json_copy(dv));
        json_t *r = cmf_seam(io, "parse_config_string_list", args);
        if (cmf_raised(r)) { if (!agent_existing) json_free(agent); return r; }
        if (r->type != JSON_ARRAY) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", cmf_py_typename(r));
            json_free(r);
            if (!agent_existing) json_free(agent);
            return cmf_make_raise("TypeError", msg);
        }
        disabled = r;
    } else {
        disabled = json_array();
    }
    bool disjoint = true;
    for (size_t i = 0; i < f->n_legacy && disjoint; i++)
        if (cmf_str_member(disabled, f->legacy[i])) disjoint = false;
    if (disjoint) {
        json_free(disabled);
        if (!agent_existing) json_free(agent);
        return json_null();   /* early return: config untouched */
    }
    json_t *new_disabled = json_array();
    for (size_t i = 0; i < disabled->c.count; i++)
        json_append(new_disabled, json_copy(disabled->c.items[i]));
    for (size_t i = 0; i < f->n_toolsets; i++)
        if (!cmf_str_member(disabled, f->toolsets[i]))
            json_append(new_disabled, json_string(f->toolsets[i]));
    json_free(disabled);
    json_set(agent, "disabled_toolsets", new_disabled);
    if (!agent_existing) json_set(config, "agent", agent);
    return json_null();
}

/* ── _record_migration ─────────────────────────────────────────────────── */
typedef struct { char *platform; json_t *sel; } cmf_selection_t;

static void cmf_selections_free(cmf_selection_t *sels, size_t n) {
    for (size_t i = 0; i < n; i++) {
        free(sels[i].platform);
        json_free(sels[i].sel);
    }
    free(sels);
}

/* PoP: cmf_record_migration @ hermes_cli/left_core_migration.py:_record_migration */
json_t *cmf_record_migration(cmf_io_t *io, const char *home, const cmf_feature_t *f) {
    char *path = cmf_path_join(home, "config.yaml");
    char *home_posix = cmf_path_posix(home);
    json_t *args = json_object();
    json_set(args, "path", json_string(path));
    json_t *r = cmf_seam(io, "config_exists", args);
    if (cmf_raised(r)) { free(path); free(home_posix); return r; }
    bool created = !cmf_truthy(r);
    json_free(r);

    args = json_object();
    json_set(args, "path", json_string(path));
    r = cmf_seam(io, "read_user_config_raw", args);
    if (cmf_raised(r)) { free(path); free(home_posix); return r; }
    json_t *config = r;   /* may be a non-dict: the first .get raises below */

    args = json_object();
    json_set(args, "plugin", json_string(f->plugin));
    json_set(args, "home", json_string(home_posix));
    r = cmf_seam(io, "plugin_present", args);
    if (cmf_raised(r)) { json_free(config); free(path); free(home_posix); return r; }
    bool present = cmf_truthy(r);
    json_free(r);

    /* live: installed = plugin_present(...) and not _marked(config, ...) —
     * _marked's config.get is the first attribute access on a non-dict. */
    if (!config || config->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", cmf_py_typename(config));
        json_free(config);
        free(path); free(home_posix);
        return cmf_make_raise("AttributeError", msg);
    }
    bool installed = present && !cmf_marked(config, "_left_core_installed", f);
    if (cmf_scope_kept(config, f) && !installed) {
        json_free(config); free(path); free(home_posix);
        return json_null();   /* nothing to record: no write */
    }
    if (installed) cmf_mark(config, "_left_core_installed", f);

    if (!cmf_scope_kept(config, f)) {
        json_t *saved = json_obj_get(config, "platform_toolsets");
        if (!saved || saved->type != JSON_OBJECT) saved = NULL;
        cmf_selection_t *sels = NULL;
        size_t n_sels = 0, cap_sels = 0;
        if (saved) {
            for (size_t i = 0; i < saved->c.count; i++) {
                args = json_object();
                json_set(args, "value", json_copy(saved->c.items[i]));
                r = cmf_seam(io, "parse_platform_toolsets_value", args);
                if (cmf_raised(r)) {
                    cmf_selections_free(sels, n_sels);
                    json_free(config); free(path); free(home_posix);
                    return r;
                }
                json_t *sel = NULL;
                if (!cmf_is_null(r)) sel = r; else json_free(r);
                if (n_sels == cap_sels) {
                    cap_sels = cap_sels ? cap_sels * 2 : 8;
                    sels = (cmf_selection_t *)realloc(sels, cap_sels * sizeof(cmf_selection_t));
                }
                sels[n_sels].platform = strdup(saved->c.keys[i]);
                sels[n_sels].sel = sel;
                n_sels++;
            }
        }
        for (size_t i = 0; i < f->n_off; i++) {
            bool found = false;
            for (size_t j = 0; j < n_sels && !found; j++)
                if (strcmp(sels[j].platform, f->off[i]) == 0) found = true;
            if (found) continue;   /* setdefault keeps the saved selection */
            if (n_sels == cap_sels) {
                cap_sels = cap_sels ? cap_sels * 2 : 8;
                sels = (cmf_selection_t *)realloc(sels, cap_sels * sizeof(cmf_selection_t));
            }
            sels[n_sels].platform = strdup(f->off[i]);
            sels[n_sels].sel = NULL;
            n_sels++;
        }
        json_t *known = json_obj_get(config, "known_plugin_toolsets");
        bool known_existing = known && known->type == JSON_OBJECT;
        if (!known_existing) known = json_object();
        for (size_t i = 0; i < n_sels; i++) {
            const char *platform = sels[i].platform;
            json_t *sel = sels[i].sel;
            bool is_off = cmf_cstr_in(f->off, f->n_off, platform);
            if (!sel && !is_off) continue;
            if (sel) {
                if (sel->type != JSON_ARRAY) {
                    char msg[128];
                    snprintf(msg, sizeof msg, "'%s' object is not iterable", cmf_py_typename(sel));
                    cmf_selections_free(sels, n_sels);
                    if (!known_existing) json_free(known);
                    json_free(config); free(path); free(home_posix);
                    return cmf_make_raise("TypeError", msg);
                }
                if (cmf_core_carried(f, sel)) continue;
            }
            json_t *cur = json_obj_get(known, platform);
            if (!cur || cur->type != JSON_ARRAY) cur = NULL;
            char **strs = NULL;
            size_t n_strs = 0, cap_strs = 0;
            bool any_missing = false;
            for (size_t t = 0; t < f->n_toolsets; t++) {
                if (cmf_str_member(cur, f->toolsets[t])) continue;
                any_missing = true;
                if (n_strs == cap_strs) { cap_strs = cap_strs ? cap_strs * 2 : 8; strs = (char **)realloc(strs, cap_strs * sizeof(char *)); }
                strs[n_strs++] = strdup(f->toolsets[t]);
            }
            if (any_missing) {
                if (cur) {
                    for (size_t k = 0; k < cur->c.count; k++) {
                        if (n_strs == cap_strs) { cap_strs = cap_strs ? cap_strs * 2 : 8; strs = (char **)realloc(strs, cap_strs * sizeof(char *)); }
                        strs[n_strs++] = cmf_py_str(cur->c.items[k]);
                    }
                }
                json_t *arr = cmf_sorted_unique(strs, n_strs);
                json_set(known, platform, arr);
            }
            for (size_t k = 0; k < n_strs; k++) free(strs[k]);
            free(strs);
        }
        if (known->c.count > 0) {
            if (!known_existing) json_set(config, "known_plugin_toolsets", known);
        } else if (!known_existing) {
            json_free(known);
        }
        cmf_selections_free(sels, n_sels);
        if (f->n_replaces > 0) {
            r = cmf_turn_off_replaced(io, config, f);
            if (cmf_raised(r)) { json_free(config); free(path); free(home_posix); return r; }
            json_free(r);
        }
        r = cmf_keep_legacy_disable(io, config, f);
        if (cmf_raised(r)) { json_free(config); free(path); free(home_posix); return r; }
        json_free(r);
        cmf_mark(config, "_left_core_scoped", f);
    }

    if (created) {
        /* _stamp_created: config["_config_version"] = DEFAULT_CONFIG[...];
         * note_first_stamp(home, {}, config) — scripted config effect. */
        r = cmf_seam(io, "default_config_version", json_object());
        if (cmf_raised(r)) { json_free(config); free(path); free(home_posix); return r; }
        json_set(config, "_config_version", r);
        args = json_object();
        json_set(args, "home", json_string(home_posix));
        json_set(args, "on_disk", json_object());
        json_set(args, "config", json_copy(config));
        r = cmf_seam(io, "note_first_stamp", args);
        if (cmf_raised(r)) { json_free(config); free(path); free(home_posix); return r; }
        if (r && r->type == JSON_OBJECT) {
            for (size_t i = 0; i < r->c.count; i++)
                json_set(config, r->c.keys[i], json_copy(r->c.items[i]));
        }
        json_free(r);
    }

    args = json_object();
    json_set(args, "path", json_string(path));
    json_set(args, "config", json_copy(config));
    r = cmf_seam(io, "atomic_config_write", args);
    json_free(config);
    free(path);
    free(home_posix);
    if (cmf_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* ── _first_cause ──────────────────────────────────────────────────────── */

/* PoP: cmf_first_cause @ hermes_cli/left_core_migration.py:_first_cause */
char *cmf_first_cause(const char *error) {
    size_t n_lines = 0;
    char **lines = cmf_splitlines(error ? error : "", &n_lines);
    char **stripped = (char **)malloc((n_lines + 1) * sizeof(char *));
    size_t n_stripped = 0;
    for (size_t i = 0; i < n_lines; i++) {
        char *s = cmf_strip(lines[i]);
        if (s && s[0]) stripped[n_stripped++] = s;
        else free(s);
        free(lines[i]);
    }
    free(lines);
    char *cause = NULL;
    for (size_t i = 0; i < n_stripped; i++) {
        if (strncmp(stripped[i], "fatal:", 6) == 0 || strncmp(stripped[i], "error:", 6) == 0) {
            const char *colon = strchr(stripped[i], ':');
            cause = cmf_strip(colon + 1);
            break;
        }
    }
    const char *base = (cause && cause[0]) ? cause
                     : (n_stripped ? stripped[0] : "unknown error");
    size_t blen = strlen(base);
    while (blen > 0 && (base[blen - 1] == '.' || base[blen - 1] == ' ')) blen--;
    char *out = (char *)malloc(blen + 1);
    if (out) { memcpy(out, base, blen); out[blen] = '\0'; }
    free(cause);
    for (size_t i = 0; i < n_stripped; i++) free(stripped[i]);
    free(stripped);
    return out;
}

/* ── _install_one ──────────────────────────────────────────────────────── */
static char *cmf_install_ok_msg(const cmf_feature_t *f) {
    cmf_buf_t b; cmf_binit(&b);
    cmf_bput(&b, "  \xe2" "\x9c" "\x93" " ");
    cmf_bput(&b, f->label);
    cmf_bput(&b, " moved out of core \xe2" "\x80" "\x94" " installed the '");
    cmf_bput(&b, f->plugin);
    cmf_bput(&b, "' plugin from the catalog (");
    cmf_bput(&b, f->unchanged);
    cmf_bput(&b, ").");
    return b.s;
}

static char *cmf_install_fail_msg(const cmf_feature_t *f, const char *cause, const char *cmd) {
    cmf_buf_t b; cmf_binit(&b);
    cmf_bput(&b, "  \xe2" "\x9a" "\xa0" " ");
    cmf_bput(&b, f->label);
    cmf_bput(&b, " moved out of core and its '");
    cmf_bput(&b, f->plugin);
    cmf_bput(&b, "' plugin could not be installed automatically: ");
    cmf_bput(&b, cause);
    cmf_bput(&b, ". Run `");
    cmf_bput(&b, cmd);
    cmf_bput(&b, "`.");
    return b.s;
}

/* _noticed: append feature.notice(home, process_env=...) when truthy. */
static json_t *cmf_noticed(cmf_io_t *io, const char *home_posix, const cmf_feature_t *f,
                           bool process_env, char **msg) {
    json_t *args = json_object();
    json_set(args, "plugin", json_string(f->plugin));
    json_set(args, "home", json_string(home_posix));
    json_set(args, "process_env", json_bool(process_env));
    json_t *r = cmf_seam(io, "feature_notice", args);
    if (cmf_raised(r)) return r;
    if (cmf_truthy(r)) {
        char *extra = cmf_py_str(r);
        cmf_buf_t b; cmf_binit(&b);
        cmf_bput(&b, *msg);
        cmf_bput(&b, " ");
        cmf_bput(&b, extra);
        free(extra);
        free(*msg);
        *msg = b.s;
    }
    json_free(r);
    return NULL;
}

/* PoP: cmf_install_one @ hermes_cli/left_core_migration.py:_install_one */
json_t *cmf_install_one(cmf_io_t *io, const char *home, const cmf_feature_t *f, bool process_env) {
    char *home_posix = cmf_path_posix(home);
    json_t *args = json_object();
    json_set(args, "plugin", json_string(f->plugin));
    json_t *r = cmf_seam(io, "install", args);
    json_t *result = NULL;
    if (cmf_raised(r)) {
        json_t *spec = cmf_raise_spec(r);
        if (!cmf_exc_is_exception(spec)) { free(home_posix); return r; }
        result = json_object();
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string(json_get_str(spec, "message", "")));
        json_free(r);
    } else {
        result = r;
    }
    if (!result || result->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", cmf_py_typename(result));
        json_free(result);
        free(home_posix);
        return cmf_make_raise("AttributeError", msg);
    }
    if (cmf_truthy(json_obj_get(result, "ok"))) {
        r = cmf_seam(io, "refresh_bound_home", json_object());
        if (cmf_raised(r)) { json_free(result); free(home_posix); return r; }
        json_free(r);
        json_t *r2 = cmf_record_migration(io, home, f);
        if (cmf_raised(r2)) {
            json_t *spec = cmf_raise_spec(r2);
            if (!cmf_exc_is_exception(spec)) {
                json_free(result); free(home_posix);
                return r2;   /* SystemExit escapes except Exception */
            }
            cmf_buf_t db; cmf_binit(&db);
            cmf_bput(&db, "left-core install marker not written for ");
            cmf_bput(&db, home_posix);
            cmf_bput(&db, ": ");
            cmf_bput(&db, json_get_str(spec, "message", ""));
            args = json_object();
            json_set(args, "message", json_string(db.s ? db.s : ""));
            free(db.s);
            json_free(r2);
            json_t *r3 = cmf_seam(io, "logger_debug", args);
            if (cmf_raised(r3)) { json_free(result); free(home_posix); return r3; }
            json_free(r3);
        } else {
            json_free(r2);
        }
        char *msg = cmf_install_ok_msg(f);
        json_t *nr = cmf_noticed(io, home_posix, f, process_env, &msg);
        if (nr) { free(msg); json_free(result); free(home_posix); return nr; }
        args = json_object();
        json_set(args, "message", json_string(msg));
        free(msg);
        r = cmf_seam(io, "say", args);
        json_free(result);
        free(home_posix);
        if (cmf_raised(r)) return r;
        json_free(r);
        return json_bool(true);
    }
    args = json_object();
    json_set(args, "home", json_string(home_posix));
    json_set(args, "plugin", json_string(f->plugin));
    r = cmf_seam(io, "note_failure", args);
    if (cmf_raised(r)) { json_free(result); free(home_posix); return r; }
    json_free(r);
    json_t *errval = json_obj_get(result, "error");
    char *errstr = cmf_truthy(errval) ? cmf_py_str(errval) : strdup("");
    char *cause = cmf_first_cause(errstr);
    free(errstr);
    args = json_object();
    json_set(args, "plugin", json_string(f->plugin));
    json_set(args, "home", json_string(home_posix));
    r = cmf_seam(io, "install_command", args);
    if (cmf_raised(r)) { free(cause); json_free(result); free(home_posix); return r; }
    char *cmd = cmf_py_str(r);
    json_free(r);
    char *msg = cmf_install_fail_msg(f, cause ? cause : "", cmd ? cmd : "");
    free(cause);
    free(cmd);
    json_t *nr = cmf_noticed(io, home_posix, f, process_env, &msg);
    if (nr) { free(msg); json_free(result); free(home_posix); return nr; }
    args = json_object();
    json_set(args, "message", json_string(msg));
    free(msg);
    r = cmf_seam(io, "say", args);
    json_free(result);
    free(home_posix);
    if (cmf_raised(r)) return r;
    json_free(r);
    return json_bool(false);
}

/* ── migrate_home ──────────────────────────────────────────────────────── */

/* PoP: cmf_migrate_home @ hermes_cli/left_core_migration.py:migrate_home */
json_t *cmf_migrate_home(cmf_io_t *io, const char *home, bool process_env) {
    char *home_posix = cmf_path_posix(home);
    json_t *args = json_object();
    json_set(args, "home", json_string(home_posix));
    json_set(args, "process_env", json_bool(process_env));
    free(home_posix);
    json_t *r = cmf_seam(io, "pending", args);
    if (cmf_raised(r)) return r;
    if (!r || r->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", cmf_py_typename(r));
        json_free(r);
        return cmf_make_raise("TypeError", msg);
    }
    json_t *installed = json_array();
    for (size_t i = 0; i < r->c.count; i++) {
        json_t *name = r->c.items[i];
        const cmf_feature_t *f = cmf_feature_lookup(
            (name && name->type == JSON_STRING) ? name->str_val : NULL);
        if (!f) {
            json_free(r);
            json_free(installed);
            return cmf_make_raise("StopIteration", "");
        }
        json_t *ok = cmf_install_one(io, home, f, process_env);
        if (cmf_raised(ok)) { json_free(r); json_free(installed); return ok; }
        if (cmf_truthy(ok)) json_append(installed, json_string(f->plugin));
        json_free(ok);
    }
    json_free(r);
    return installed;
}/* ── version_token ─────────────────────────────────────────────────────── */
/* Tagged element repr ({"int":n} / {"float":x} / {"str":s} / {"bool":b} /
 * {"null":true} / {"tuple":[...]}): the tags preserve the int/float identity
 * a parsed JSON number loses, which the live isinstance(version[0], int)
 * branch and repr(version) both depend on. */
static char *cmf_tagged_repr(const json_t *elem) {
    cmf_buf_t b; cmf_binit(&b);
    if (!elem || elem->type != JSON_OBJECT) { cmf_bput(&b, "None"); return b.s; }
    json_t *v;
    if ((v = json_obj_get(elem, "int"))) {
        char nb[40];
        snprintf(nb, sizeof nb, "%.0f", v->type == JSON_NUMBER ? v->num_val : 0.0);
        cmf_bput(&b, nb);
    } else if ((v = json_obj_get(elem, "float"))) {
        char *fs = cmf_repr_float(v->type == JSON_NUMBER ? v->num_val : 0.0);
        cmf_bput(&b, fs);
        free(fs);
    } else if ((v = json_obj_get(elem, "str"))) {
        char *rs = cmf_repr_str(v->type == JSON_STRING ? v->str_val : "");
        cmf_bput(&b, rs);
        free(rs);
    } else if ((v = json_obj_get(elem, "bool"))) {
        cmf_bput(&b, (v->type == JSON_BOOL && v->bool_val) ? "True" : "False");
    } else if (json_obj_get(elem, "null")) {
        cmf_bput(&b, "None");
    } else if ((v = json_obj_get(elem, "tuple"))) {
        cmf_bputc(&b, '(');
        size_t n = (v->type == JSON_ARRAY) ? v->c.count : 0;
        for (size_t i = 0; i < n; i++) {
            if (i) cmf_bput(&b, ", ");
            char *rs = cmf_tagged_repr(v->c.items[i]);
            cmf_bput(&b, rs);
            free(rs);
        }
        if (n == 1) cmf_bputc(&b, ',');
        cmf_bputc(&b, ')');
    } else {
        cmf_bput(&b, "None");
    }
    return b.s;
}

/* PoP: cmf_version_token @ hermes_cli/config_backend.py:version_token */
json_t *cmf_version_token(cmf_io_t *io, const json_t *version) {
    (void)io;
    if (!version || version->type != JSON_ARRAY || version->c.count == 0)
        return json_number(0);
    const json_t *head = version->c.items[0];
    bool int_led = head && head->type == JSON_OBJECT &&
                   (json_obj_get(head, "int") || json_obj_get(head, "bool"));
    if (int_led) {
        double v = 0.0;
        json_t *iv = json_obj_get(head, "int");
        json_t *bv = json_obj_get(head, "bool");
        if (iv && iv->type == JSON_NUMBER) v = iv->num_val;
        else if (bv && bv->type == JSON_BOOL) v = bv->bool_val ? 1.0 : 0.0;
        return json_number(v / 1000000000.0);
    }
    cmf_buf_t b; cmf_binit(&b);
    cmf_bputc(&b, '(');
    for (size_t i = 0; i < version->c.count; i++) {
        if (i) cmf_bput(&b, ", ");
        char *rs = cmf_tagged_repr(version->c.items[i]);
        cmf_bput(&b, rs);
        free(rs);
    }
    if (version->c.count == 1) cmf_bputc(&b, ',');
    cmf_bputc(&b, ')');
    uint32_t crc = cmf_crc32(b.s ? b.s : "", b.len);
    free(b.s);
    return json_number((double)(crc ? crc : 1u));
}

/* ── _backend_for ──────────────────────────────────────────────────────── */
/* _KIND_RE = re.compile(r"[a-z][a-z0-9_]{0,63}") fullmatch */
static bool cmf_kind_valid(const char *kind) {
    size_t n = strlen(kind);
    if (n < 1 || n > 64) return false;
    if (!(kind[0] >= 'a' && kind[0] <= 'z')) return false;
    for (size_t i = 1; i < n; i++) {
        char c = kind[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    }
    return true;
}

static char *cmf_backend_unknown_msg(const char *kind) {
    char *kr = cmf_repr_str(kind);
    cmf_buf_t b; cmf_binit(&b);
    cmf_bput(&b, "HERMES_CONFIG_BACKEND=");
    cmf_bput(&b, kr);
    cmf_bput(&b, " is not a config backend this build provides (expected 'file' or 'remote').");
    free(kr);
    return b.s;
}

static bool cmf_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* PoP: cmf_backend_for @ hermes_cli/config_backend.py:_backend_for */
json_t *cmf_backend_for(cmf_io_t *io, const char *kind) {
    if (strcmp(kind, "file") == 0) {
        json_t *desc = json_object();
        json_set(desc, "name", json_string("file"));
        json_t *out = json_object();
        json_set(out, "backend", desc);
        return out;
    }
    char *unknown = cmf_backend_unknown_msg(kind);
    if (!cmf_kind_valid(kind)) {
        json_t *r = cmf_make_raise_code_str("ConfigBackendUnavailable", unknown);
        free(unknown);
        return r;
    }
    cmf_buf_t nb; cmf_binit(&nb);
    cmf_bput(&nb, "plugins.config_backends.");
    cmf_bput(&nb, kind);
    char *name = nb.s;

    json_t *args = json_object();
    json_set(args, "name", json_string(name));
    json_t *r = cmf_seam(io, "sys_modules_get", args);
    if (cmf_raised(r)) { free(unknown); free(name); return r; }
    json_t *mod = NULL;
    if (r && r->type == JSON_OBJECT) mod = r;
    else json_free(r);

    if (!mod) {
        args = json_object();
        json_set(args, "name", json_string(name));
        r = cmf_seam(io, "import_module", args);
        if (cmf_raised(r)) {
            json_t *spec = cmf_raise_spec(r);
            if (strcmp(json_get_str(spec, "type", ""), "ImportError") != 0) {
                free(unknown); free(name);
                return r;   /* only ImportError is caught */
            }
            const char *exc_name = json_get_str(spec, "name", NULL);
            bool cond1 = !exc_name || strcmp(exc_name, name) != 0;
            char prefix[256];
            snprintf(prefix, sizeof prefix, "%s.", exc_name ? exc_name : "None");
            bool cond2 = !cmf_starts_with(name, prefix);
            if (cond1 && cond2) {
                free(unknown); free(name);
                return r;   /* the backend exists but is broken: show that */
            }
            json_free(r);
            json_t *out = cmf_make_raise_code_str("ConfigBackendUnavailable", unknown);
            free(unknown); free(name);
            return out;
        }
        if (r && r->type == JSON_OBJECT) mod = r;
        else json_free(r);
    }
    free(name);
    if (!mod) {
        json_t *out = cmf_make_raise_code_str("ConfigBackendUnavailable", unknown);
        free(unknown);
        return out;
    }
    json_t *gb = json_obj_get(mod, "get_backend");
    if (!gb || gb->type != JSON_STRING || strcmp(gb->str_val ? gb->str_val : "", "fn") != 0) {
        json_free(mod);
        json_t *out = cmf_make_raise_code_str("ConfigBackendUnavailable", unknown);
        free(unknown);
        return out;
    }
    json_free(mod);
    free(unknown);
    args = json_object();
    cmf_binit(&nb);
    cmf_bput(&nb, "plugins.config_backends.");
    cmf_bput(&nb, kind);
    json_set(args, "name", json_string(nb.s ? nb.s : ""));
    free(nb.s);
    r = cmf_seam(io, "get_backend_call", args);
    if (cmf_raised(r) || cmf_is_null(r)) return r;   /* None rides unwrapped */
    json_t *out = json_object();
    json_set(out, "backend", r);
    return out;
}

/* ── _bootstrap / _ensure_bootstrap_env ────────────────────────────────── */

/* PoP: cmf_bootstrap @ hermes_cli/config_backend.py:_bootstrap */
json_t *cmf_bootstrap(cmf_io_t *io, const char *home, const char *project_env, bool once) {
    /* (once and _BOOTSTRAPPED) short-circuits BEFORE the ident read. */
    if (once && cmf_state_bootstrapped(io)) return json_null();
    json_t *owner = cmf_state_owner(io);   /* borrowed */
    json_t *r = cmf_seam(io, "threading_get_ident", json_object());
    if (cmf_raised(r)) return r;
    if (cmf_json_eq(owner, r)) { json_free(r); return json_null(); }
    json_free(r);
    r = cmf_seam(io, "threading_get_ident", json_object());
    if (cmf_raised(r)) return r;
    if (io && io->state) json_set(io->state, "_BOOTSTRAP_OWNER", json_copy(r));
    json_free(r);

    json_t *args = json_object();
    if (home) {
        char *hp = cmf_path_posix(home);
        json_set(args, "home", json_string(hp));
        free(hp);
    } else {
        json_set(args, "home", json_null());
    }
    if (project_env) {
        char *pp = cmf_path_posix(project_env);
        json_set(args, "project_env", json_string(pp));
        free(pp);
    } else {
        json_set(args, "project_env", json_null());
    }
    r = cmf_seam(io, "apply_config_bootstrap_env", args);
    bool propagate = false;
    if (cmf_raised(r)) {
        if (strcmp(json_get_str(cmf_raise_spec(r), "type", ""), "ImportError") == 0)
            json_free(r);   /* except ImportError: pass */
        else
            propagate = true;
    } else {
        json_free(r);
    }
    /* finally: complete only once the deployment is published */
    if (io && io->state) {
        json_set(io->state, "_BOOTSTRAP_OWNER", json_null());
        json_set(io->state, "_BOOTSTRAPPED", json_bool(true));
    }
    if (propagate) return r;
    return json_null();
}

/* PoP: cmf_ensure_bootstrap_env @ hermes_cli/config_backend.py:_ensure_bootstrap_env */
json_t *cmf_ensure_bootstrap_env(cmf_io_t *io) {
    if (!cmf_state_bootstrapped(io))
        return cmf_bootstrap(io, NULL, NULL, true);
    return json_null();
}

/* ── get_config_backend ────────────────────────────────────────────────── */

/* PoP: cmf_get_config_backend @ hermes_cli/config_backend.py:get_config_backend */
json_t *cmf_get_config_backend(cmf_io_t *io) {
    json_t *r = cmf_ensure_bootstrap_env(io);
    if (cmf_raised(r)) return r;
    json_free(r);
    json_t *args = json_object();
    json_set(args, "key", json_string("HERMES_CONFIG_BACKEND"));
    json_set(args, "default", json_string(""));
    r = cmf_seam(io, "os_environ_get", args);
    if (cmf_raised(r)) return r;
    if (!r || r->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", cmf_py_typename(r));
        json_free(r);
        return cmf_make_raise("AttributeError", msg);
    }
    char *s = cmf_strip(r->str_val ? r->str_val : "");
    json_free(r);
    char *low = cmf_lower(s);
    free(s);
    json_t *out = cmf_backend_for(io, (low && low[0]) ? low : "file");
    free(low);
    return out;
}

/* ── _route ────────────────────────────────────────────────────────────── */
/* Returns {"backend": descriptor, "home": posix parent} or a raise. */

/* PoP: cmf_route @ hermes_cli/config_backend.py:_route */
json_t *cmf_route(cmf_io_t *io, const char *config_path) {
    char *name = cmf_path_name(config_path);
    char *parent = cmf_path_parent(config_path);
    json_t *out = json_object();
    if (strcmp(name, "config.yaml") == 0) {
        free(name);
        json_t *b = cmf_get_config_backend(io);
        if (cmf_raised(b)) { free(parent); json_free(out); return b; }
        json_set(out, "backend", b);
    } else {
        free(name);
        char *posix = cmf_path_posix(config_path);
        json_t *desc = json_object();
        json_set(desc, "explicit", json_string(posix));
        free(posix);
        json_t *b = json_object();
        json_set(b, "backend", desc);
        json_set(out, "backend", b);
    }
    json_set(out, "home", json_string(parent));
    free(parent);
    return out;
}

/* ── read_config_doc ───────────────────────────────────────────────────── */

/* PoP: cmf_read_config_doc_flow @ hermes_cli/config_backend.py:read_config_doc */
json_t *cmf_read_config_doc_flow(cmf_io_t *io, const char *config_path) {
    json_t *route = cmf_route(io, config_path);
    if (cmf_raised(route)) return route;
    json_t *args = json_object();
    json_set(args, "home", json_string(json_get_str(route, "home", "")));
    json_free(route);
    return cmf_seam(io, "backend_read_user_doc", args);
}

/* ── FileBackend.write_path ────────────────────────────────────────────── */

/* PoP: cmf_write_path @ hermes_cli/config_backend.py:write_path */
json_t *cmf_write_path(cmf_io_t *io, const char *path, const json_t *changes) {
    char *posix = cmf_path_posix(path);
    json_t *doc = json_obj_get(changes, "document");
    if (!cmf_is_null(doc)) {
        json_t *args = json_object();
        json_set(args, "path", json_string(posix));
        json_set(args, "document", json_copy(doc));
        json_t *extra = json_obj_get(changes, "extra_content_on_create");
        json_set(args, "extra_content_on_create", extra ? json_copy(extra) : json_null());
        json_t *r = cmf_seam(io, "atomic_roundtrip_yaml_save", args);
        if (cmf_raised(r)) { free(posix); return r; }
        json_free(r);
    }
    json_t *set = json_obj_get(changes, "set");
    if (set && set->type == JSON_OBJECT) {
        for (size_t i = 0; i < set->c.count; i++) {
            json_t *args = json_object();
            json_set(args, "path", json_string(posix));
            json_set(args, "key", json_string(set->c.keys[i]));
            json_set(args, "value", json_copy(set->c.items[i]));
            json_t *r = cmf_seam(io, "atomic_roundtrip_yaml_update", args);
            if (cmf_raised(r)) { free(posix); return r; }
            json_free(r);
        }
    }
    json_t *unset = json_obj_get(changes, "unset");
    if (unset && unset->type == JSON_ARRAY) {
        for (size_t i = 0; i < unset->c.count; i++) {
            json_t *key = unset->c.items[i];
            json_t *args = json_object();
            json_set(args, "path", json_string(posix));
            json_set(args, "key", json_string((key && key->type == JSON_STRING) ? key->str_val : ""));
            json_set(args, "value", json_null());
            json_t *r = cmf_seam(io, "atomic_roundtrip_yaml_update", args);
            if (cmf_raised(r)) { free(posix); return r; }
            json_free(r);
        }
    }
    json_t *args = json_object();
    json_set(args, "path", json_string(posix));
    free(posix);
    json_t *r = cmf_seam(io, "drop_other_access", args);
    if (cmf_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* ── config_cache_version_or_none ──────────────────────────────────────── */

/* PoP: cmf_config_cache_version_or_none @ hermes_cli/config_backend.py:config_cache_version_or_none */
json_t *cmf_config_cache_version_or_none(cmf_io_t *io, const char *config_path) {
    json_t *args = json_object();
    json_set(args, "config_path", json_string(config_path));
    json_t *r = cmf_seam(io, "config_cache_version", args);
    if (cmf_raised(r)) {
        if (cmf_exc_is_oserror(cmf_raise_spec(r))) {
            json_free(r);
            return json_null();
        }
        return r;
    }
    return r;
}

/* ── write_config_keys ─────────────────────────────────────────────────── */

/* PoP: cmf_write_config_keys @ hermes_cli/config_backend.py:write_config_keys */
json_t *cmf_write_config_keys(cmf_io_t *io, const char *config_path, const json_t *updates) {
    json_t *sets = json_object();
    json_t *unset = json_array();
    if (updates && updates->type == JSON_OBJECT) {
        for (size_t i = 0; i < updates->c.count; i++) {
            json_t *v = updates->c.items[i];
            if (cmf_is_null(v))
                json_append(unset, json_string(updates->c.keys[i]));
            else
                json_set(sets, updates->c.keys[i], json_copy(v));
        }
    }
    json_t *args = json_object();
    json_set(args, "config_path", json_string(config_path));
    json_set(args, "sets", json_copy(sets));
    json_t *r = cmf_seam(io, "refuse_redacted_sets", args);
    if (cmf_raised(r)) { json_free(sets); json_free(unset); return r; }
    json_free(r);
    json_t *route = cmf_route(io, config_path);
    if (cmf_raised(route)) { json_free(sets); json_free(unset); return route; }
    json_t *changes = json_object();
    json_set(changes, "document", json_null());
    json_set(changes, "set", sets);
    json_set(changes, "unset", unset);
    json_set(changes, "extra_content_on_create", json_null());
    args = json_object();
    json_set(args, "home", json_string(json_get_str(route, "home", "")));
    json_set(args, "changes", changes);
    json_free(route);
    r = cmf_seam(io, "backend_write_changes", args);
    if (cmf_raised(r)) return r;
    json_free(r);
    return json_null();
}