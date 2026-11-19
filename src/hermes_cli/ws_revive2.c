/*
 * ws_revive2.c — deep-stub revival group 2 (web_server_wrappers.c cluster).
 *
 * Faithful ports of 19 PoP-tagged empty-alloc stubs, extracted from
 * src/hermes_cli/web_server_wrappers.c under their ORIGINAL cfn names and
 * signatures (json_t *fn(json_t *req)). The stub definitions were deleted
 * from the wrappers file; the PoP tags moved here with the definitions.
 *
 * Universal seam contract
 * -----------------------
 * All I/O rides one injected callback installed per case by the differential
 * harness (tests/t_port_ws_revive2.c #includes this file):
 *
 *   g_ws2_io->call(io, name, args)
 *
 * - `name` identifies the live call (see the seam table below); `args` is a
 *   borrowed-on-build JSON object whose ownership TRANSFERS to the seam —
 *   the caller must NOT free it after the call (the harness dispatcher
 *   adopts it into the trace event).
 * - The result is a NEW json_t owned by the caller: NULL == Python None,
 *   {"__raise__": Type, "message": str(exc)} == a Python exception. Ports
 *   propagate or swallow it exactly where the live code does; `except
 *   Exception` sites pass the BaseException kinds (SystemExit,
 *   KeyboardInterrupt, GeneratorExit, CancelledError) through.
 * - Ordered {ev, args, ret} events are recorded by the dispatcher into
 *   io->trace, identical in shape to the oracle's recording monkeypatches.
 * - No seam installed (production default) is FAIL-CLOSED: ws2_seam frees
 *   the args and returns a RuntimeError raise spec.
 *
 * Modeling conventions (documented divergences are fixture-bounded):
 * - Integral JSON numbers model Python ints (libjson stores doubles);
 *   non-integral numbers use Python float repr (shortest round-trip).
 * - str()/repr() of containers is transcribed exactly (Python syntax);
 *   case folding is ASCII-only (no Unicode fold maps onto the ASCII tag
 *   literals these ports compare); \d digit filters are ASCII (fixtures
 *   stay inside ASCII digits).
 * - String slicing/len are UTF-8 codepoint-aware; strip() covers the full
 *   Unicode White_Space set.
 * - Module globals mutated by live code (_dashboard_plugins_cache, the
 *   loaded config) ride the case input object as state keys ("cache",
 *   "config"); the harness echoes req minus "io" as the post-state.
 *
 * Seam table (name -> live call):
 *   env_lookup                        -> web_server_memory._env_lookup(env_key)
 *   resp.json                         -> oauth resp.json()
 *   discover_memory_provider_statuses -> web_server_memory._discover_memory_provider_statuses()
 *   environ.get                       -> os.environ.get(key, default)
 *   ensure_managed_root               -> web_server_files._ensure_managed_root(raw_path)
 *   canonical_path                    -> web_server_files._canonical_path(path)
 *   default_hermes_root_is_opt_data   -> web_server_files._default_hermes_root_is_opt_data()
 *   path_home                         -> pathlib.Path.home()
 *   get_active_profile_name           -> hermes_cli.profiles.get_active_profile_name()
 *   resolve_profile_dir               -> web_server_profiles._resolve_profile_dir(profile)
 *   default_gateway_multiplexes       -> gateway_multiplex_mode.default_gateway_multiplexes()
 *   detect_provider_for_model         -> hermes_cli.models.detect_provider_for_model(model, prev_provider)
 *   normalize_provider                -> hermes_cli.models.normalize_provider(provider)
 *   provider_has_credentials          -> models_detect.provider_has_credentials(provider)
 *   secondary_profile_home            -> web_server_plugin_profiles.secondary_profile_home()
 *   profile_dashboard_plugins         -> web_server_plugin_profiles.profile_dashboard_plugins(home, force_rescan)
 *   path_is_dir                       -> pathlib.Path(path).is_dir()
 *   discover_dashboard_plugins        -> web_server._discover_dashboard_plugins()
 *   write_provider_flat               -> memory_providers._write_provider_flat(provider, values)
 *   write_provider_own                -> memory_providers._write_provider_own(provider, values)
 *   load_config                       -> hermes_cli.config.load_config()
 *   save_config                       -> hermes_cli.config.save_config(config)
 *   read_text                         -> Path(session_path / "creds.json").read_text(encoding="utf-8-sig")
 *                                        (one leading UTF-8 BOM stripped by both dispatchers)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "hermes_json.h"

typedef struct ws2_io {
    json_t *(*call)(struct ws2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ws2_io_t;

/* Installed per case by the harness; NULL = fail-closed production default. */
static ws2_io_t *g_ws2_io = NULL;

/* ── raise specs ── */

static json_t *ws2_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

static int ws2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int ws2_is_base_exc(const json_t *r) {
    if (!ws2_is_raise(r)) return 0;
    const char *t = json_get_str(r, "__raise__", "");
    return strcmp(t, "SystemExit") == 0 || strcmp(t, "KeyboardInterrupt") == 0 ||
           strcmp(t, "GeneratorExit") == 0 || strcmp(t, "CancelledError") == 0;
}

/* Ownership of a propagated raise spec passes to the return value. The
 * oracle's run_case wrapper adds SystemExit.code (== str(exc) == message
 * for a scripted SystemExit(msg)); mirror that here, bare otherwise. */
static json_t *ws2_raise_out(json_t *r) {
    if (ws2_is_raise(r)) {
        const char *t = json_get_str(r, "__raise__", "");
        if (strcmp(t, "SystemExit") == 0)
            json_set_hdr(r, "code", json_get_str(r, "message", ""));
    }
    return r;
}

/* HTTPException str() is "<status_code>: <detail>" (starlette). */
static json_t *ws2_http_exc(int status, const char *detail) {
    char msg[1024];
    snprintf(msg, sizeof msg, "%d: %s", status, detail ? detail : "");
    return ws2_raise("HTTPException", msg);
}

/* str(KeyError(k)) is repr(k). */
static json_t *ws2_key_error_str(const char *key) {
    char msg[512];
    snprintf(msg, sizeof msg, "'%s'", key ? key : "");
    return ws2_raise("KeyError", msg);
}

static json_t *ws2_seam(const char *name, json_t *args) {
    if (!g_ws2_io || !g_ws2_io->call) {
        json_free(args); /* no seam takes ownership on the fallback path */
        char msg[256];
        snprintf(msg, sizeof msg, "no seam installed for %s", name);
        return ws2_raise("RuntimeError", msg);
    }
    return g_ws2_io->call(g_ws2_io, name, args); /* seam owns args */
}

/* ── truthiness / types ── */

static int ws2_truthy(const json_t *v) {
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

static int ws2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static int ws2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int ws2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int ws2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int ws2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int ws2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int ws2_is_integral(double d) {
    return isfinite(d) && d == trunc(d) && fabs(d) < 1e16;
}

/* Python type name of a JSON value (integral doubles model ints). */
static const char *ws2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return ws2_is_integral(v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── UTF-8 codepoint helpers ── */

static unsigned ws2_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
             ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

static size_t ws2_cplen(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    while (*s) { ws2_cp_next(&s); n++; }
    return n;
}

/* Byte offset of codepoint index n (clamped to length). */
static size_t ws2_cpoff(const char *s, size_t n) {
    size_t i = 0;
    const char *p = s ? s : "";
    while (*p && i < n) { ws2_cp_next(&p); i++; }
    return (size_t)(p - (s ? s : ""));
}

/* Codepoint slice [start, end) — Python s[start:end] with non-negative
 * indices clamped to length. Caller frees. */
static char *ws2_slice(const char *s, size_t start, size_t end) {
    if (!s) s = "";
    size_t len = ws2_cplen(s);
    if (start > len) start = len;
    if (end > len) end = len;
    if (end < start) end = start;
    size_t a = ws2_cpoff(s, start), b = ws2_cpoff(s, end);
    char *r = malloc(b - a + 1);
    if (r) { memcpy(r, s + a, b - a); r[b - a] = '\0'; }
    return r;
}

static int ws2_isspace_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* str.isprintable() approximation for repr(): Cc/Cf/Zl/Zp/Zs non-printable,
 * other non-ASCII codepoints default printable (fixtures stay in range). */
static int ws2_isprint_cp(unsigned cp) {
    if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) return 0;
    if (cp < 0x7F) return 1;
    if (cp == 0xAD || cp == 0x180E || cp == 0x2028 || cp == 0x2029) return 0;
    if (cp >= 0x600 && cp <= 0x605) return 0;
    if (cp == 0x61C || cp == 0x6DD || cp == 0x70F || cp == 0x8E2) return 0;
    if (cp >= 0x200B && cp <= 0x200F) return 0;
    if (cp >= 0x202A && cp <= 0x202E) return 0;
    if (cp >= 0x2060 && cp <= 0x2064) return 0;
    if (cp >= 0x2066 && cp <= 0x206F) return 0;
    if (cp == 0xFEFF || (cp >= 0xFFF9 && cp <= 0xFFFB)) return 0;
    if (cp == 0xA0 || cp == 0x1680) return 0;
    if (cp >= 0x2000 && cp <= 0x200A) return 0;
    if (cp == 0x202F || cp == 0x205F || cp == 0x3000) return 0;
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0;
    if (cp >= 0xFDD0 && cp <= 0xFDEF) return 0;
    if ((cp & 0xFFFE) == 0xFFFE) return 0;
    if (cp >= 0xE000 && cp <= 0xF8FF) return 0;
    return 1;
}

/* str.strip() — both ends, Unicode White_Space. Caller frees. */
static char *ws2_strip(const char *s) {
    if (!s) s = "";
    const char *a = s;
    const char *end = s + strlen(s);
    while (*a) {
        const char *nx = a;
        unsigned cp = ws2_cp_next(&nx);
        if (!ws2_isspace_cp(cp)) break;
        a = nx;
    }
    const char *b = end;
    while (b > a) {
        const char *p = b;
        do { p--; } while (p > a && ((unsigned char)*p & 0xC0) == 0x80);
        const char *nx = p;
        unsigned cp = ws2_cp_next(&nx);
        if (!ws2_isspace_cp(cp) || nx != b) break;
        b = p;
    }
    size_t n = (size_t)(b - a);
    char *r = malloc(n + 1);
    if (r) { memcpy(r, a, n); r[n] = '\0'; }
    return r;
}

/* ASCII case fold — exact for the ASCII literals these ports compare. */
static char *ws2_lower(const char *s) {
    char *r = strdup(s ? s : "");
    if (r) for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}

static char *ws2_strdup(const char *s) { return strdup(s ? s : ""); }

/* Append helper for building strings. */
static void ws2_sappend(char **buf, size_t *len, size_t *cap, const char *s) {
    size_t n = strlen(s);
    if (*len + n + 1 > *cap) {
        size_t nc = (*cap ? *cap : 64);
        while (nc < *len + n + 1) nc *= 2;
        char *nb = realloc(*buf, nc);
        if (!nb) return;
        *buf = nb; *cap = nc;
    }
    memcpy(*buf + *len, s, n + 1);
    *len += n;
}
/* ── Python str() / repr() of JSON values ── */

/* Python float repr: shortest round-trip digits, positional notation keeps
 * a ".0" tail, exponent form matches %g (e+NN / e-NN, >=2 exponent digits
 * come out of %g already for the ranges fixtures use). */
static char *ws2_float_repr(double v) {
    char buf[64];
    if (isnan(v)) return ws2_strdup("nan");
    if (isinf(v)) return ws2_strdup(v > 0 ? "inf" : "-inf");
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, v);
        if (strtod(buf, NULL) == v) break;
    }
    char *r = ws2_strdup(buf);
    if (!strchr(r, '.') && !strchr(r, 'e') && !strchr(r, 'E')) {
        char *nb = malloc(strlen(r) + 3);
        if (nb) { sprintf(nb, "%s.0", r); free(r); r = nb; }
    }
    return r;
}

static char *ws2_pyrepr(const json_t *v);

/* repr of a string node: Python quote selection + escapes. */
static char *ws2_str_repr(const char *s) {
    if (!s) s = "";
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    char *buf = NULL; size_t len = 0, cap = 0;
    char tmp[2] = {q, '\0'};
    ws2_sappend(&buf, &len, &cap, tmp);
    const char *p = s;
    while (*p) {
        const char *nx = p;
        unsigned cp = ws2_cp_next(&nx);
        size_t bytes = (size_t)(nx - p);
        char esc[16];
        if (cp == (unsigned)q || cp == '\\') {
            esc[0] = '\\'; esc[1] = (char)cp; esc[2] = '\0';
            ws2_sappend(&buf, &len, &cap, esc);
        } else if (cp == '\n') ws2_sappend(&buf, &len, &cap, "\\n");
        else if (cp == '\r') ws2_sappend(&buf, &len, &cap, "\\r");
        else if (cp == '\t') ws2_sappend(&buf, &len, &cap, "\\t");
        else if (cp < 0x20 || cp == 0x7F) {
            snprintf(esc, sizeof esc, "\\x%02x", cp);
            ws2_sappend(&buf, &len, &cap, esc);
        } else if (cp < 0x80) {
            char c[2] = {(char)cp, '\0'};
            ws2_sappend(&buf, &len, &cap, c);
        } else if (ws2_isprint_cp(cp)) {
            char *raw = malloc(bytes + 1);
            if (raw) { memcpy(raw, p, bytes); raw[bytes] = '\0'; ws2_sappend(&buf, &len, &cap, raw); free(raw); }
        } else if (cp <= 0xFFFF) {
            snprintf(esc, sizeof esc, "\\u%04x", cp);
            ws2_sappend(&buf, &len, &cap, esc);
        } else {
            snprintf(esc, sizeof esc, "\\U%08x", cp);
            ws2_sappend(&buf, &len, &cap, esc);
        }
        p = nx;
    }
    tmp[0] = q;
    ws2_sappend(&buf, &len, &cap, tmp);
    return buf ? buf : ws2_strdup("''");
}

static char *ws2_num_str(const json_t *v) {
    double d = v->num_val;
    if (ws2_is_integral(d)) {
        char buf[64];
        if (d == 0.0) return ws2_strdup("0"); /* int(-0.0) == 0 */
        snprintf(buf, sizeof buf, "%.0f", d);
        return ws2_strdup(buf);
    }
    return ws2_float_repr(d);
}

/* repr(): strings quoted, containers in Python syntax. */
static char *ws2_pyrepr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ws2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ws2_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return ws2_num_str(v);
        case JSON_STRING: return ws2_str_repr(v->str_val);
        case JSON_ARRAY: {
            char *buf = NULL; size_t len = 0, cap = 0;
            ws2_sappend(&buf, &len, &cap, "[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ws2_sappend(&buf, &len, &cap, ", ");
                char *it = ws2_pyrepr(v->c.items[i]);
                ws2_sappend(&buf, &len, &cap, it);
                free(it);
            }
            ws2_sappend(&buf, &len, &cap, "]");
            return buf ? buf : ws2_strdup("[]");
        }
        case JSON_OBJECT: {
            char *buf = NULL; size_t len = 0, cap = 0;
            ws2_sappend(&buf, &len, &cap, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ws2_sappend(&buf, &len, &cap, ", ");
                char *k = ws2_str_repr(v->c.keys[i]);
                char *val = ws2_pyrepr(v->c.items[i]);
                ws2_sappend(&buf, &len, &cap, k);
                ws2_sappend(&buf, &len, &cap, ": ");
                ws2_sappend(&buf, &len, &cap, val);
                free(k); free(val);
            }
            ws2_sappend(&buf, &len, &cap, "}");
            return buf ? buf : ws2_strdup("{}");
        }
        default: return ws2_strdup("None");
    }
}

/* str(): strings raw, everything else repr-shaped. */
static char *ws2_pystr(const json_t *v) {
    if (ws2_is_str(v)) return ws2_strdup(v->str_val);
    return ws2_pyrepr(v);
}

/* ── Python == on JSON values (bool <-> number compares numerically) ── */

static int ws2_eq(const json_t *a, const json_t *b) {
    int an = ws2_is_none(a), bn = ws2_is_none(b);
    if (an || bn) return an && bn;
    int anum = ws2_is_num(a) || ws2_is_bool(a);
    int bnum = ws2_is_num(b) || ws2_is_bool(b);
    if (anum && bnum) {
        double x = ws2_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = ws2_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!ws2_eq(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !ws2_eq(a->c.items[i], bv)) return 0;
            }
            return 1;
        default: return 0;
    }
}

/* value in (None, "") — the membership test the memory helpers use. */
static int ws2_none_or_empty(const json_t *v) {
    if (ws2_is_none(v)) return 1;
    return ws2_is_str(v) && (!v->str_val || !v->str_val[0]);
}

/* ── addition with Python TypeError messages ── */

/* Live TypeError text for a failed + / += : str on the LEFT is the
 * concatenation message (str.__add__/__iadd__), everything else the
 * generic binary-op message with the actual operator. */
static void ws2_add_msg(char *msg, size_t cap, const json_t *a, const json_t *b,
                        const char *op) {
    if (ws2_is_str(a))
        snprintf(msg, cap, "can only concatenate str (not \"%s\") to str",
                 ws2_type_name(b));
    else
        snprintf(msg, cap, "unsupported operand type(s) for %s: '%s' and '%s'",
                 op, ws2_type_name(a), ws2_type_name(b));
}

/* Numeric + / += (bool coerces numerically). Used where the result feeds a
 * numeric sort key (_token_volume). */
static double ws2_num_add(const json_t *a, const json_t *b, const char *op,
                          json_t **err) {
    int aok = ws2_is_num(a) || ws2_is_bool(a);
    int bok = ws2_is_num(b) || ws2_is_bool(b);
    if (!aok || !bok) {
        char msg[256];
        ws2_add_msg(msg, sizeof msg, a, b, op);
        *err = ws2_raise("TypeError", msg);
        return 0.0;
    }
    double x = ws2_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
    double y = ws2_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
    return x + y;
}

/* Full + / += on JSON values: numbers add, str+str concatenates, anything
 * else raises the live TypeError. Returns a NEW node, or NULL with *err. */
static json_t *ws2_add(const json_t *a, const json_t *b, const char *op,
                       json_t **err) {
    *err = NULL;
    int aok = ws2_is_num(a) || ws2_is_bool(a);
    int bok = ws2_is_num(b) || ws2_is_bool(b);
    if (aok && bok)
        return json_number(ws2_num_add(a, b, op, err));
    if (ws2_is_str(a) && ws2_is_str(b)) {
        const char *sa = a->str_val ? a->str_val : "";
        const char *sb = b->str_val ? b->str_val : "";
        size_t la = strlen(sa), lb = strlen(sb);
        char *buf = malloc(la + lb + 1);
        if (!buf) return json_string("");
        memcpy(buf, sa, la);
        memcpy(buf + la, sb, lb + 1);
        json_t *r = json_string(buf);
        free(buf);
        return r;
    }
    char msg[256];
    ws2_add_msg(msg, sizeof msg, a, b, op);
    *err = ws2_raise("TypeError", msg);
    return NULL;
}

/* Python dict-key identity for a TRUTHY JSON value: strings key by text,
 * numbers/bools by numeric value (5 == 5.0 == True share one key), so an
 * int task 5 and a str task "5" stay DISTINCT entries. NULL = unhashable
 * (list/dict). Caller frees. */
static char *ws2_dict_key(const json_t *v) {
    if (ws2_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s);
        char *k = malloc(n + 3);
        if (!k) return NULL;
        k[0] = 's'; k[1] = ':';
        memcpy(k + 2, s, n + 1);
        return k;
    }
    if (ws2_is_num(v) || ws2_is_bool(v)) {
        double d = ws2_is_bool(v) ? (v->bool_val ? 1.0 : 0.0) : v->num_val;
        char buf[64];
        snprintf(buf, sizeof buf, "n:%.17g", d);
        return ws2_strdup(buf);
    }
    return NULL;
}

/* ── float() / int() coercion ── */

static int ws2_ascii_digit(unsigned char c) { return c >= '0' && c <= '9'; }

/* Scan a digit run with Python underscore rules (each '_' between two
 * digits). Appends digits to out (may be NULL to just validate). Returns
 * chars consumed, or -1 on a bad underscore. */
static long ws2_digit_run(const char *p, char *out, size_t *outlen) {
    long n = 0;
    int prev_digit = 0;
    while (p[n]) {
        unsigned char c = (unsigned char)p[n];
        if (ws2_ascii_digit(c)) {
            if (out) out[(*outlen)++] = (char)c;
            prev_digit = 1; n++;
        } else if (c == '_') {
            if (!prev_digit || !ws2_ascii_digit((unsigned char)p[n + 1])) return -1;
            prev_digit = 0; n++;
        } else break;
    }
    return n;
}

/* Python float(value). Returns 0 on success, -1 with *err set (a raise
 * spec) on TypeError/ValueError. bool coerces numerically here; callers
 * that special-case bool do so before calling. */
static int ws2_float_of(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (ws2_is_num(v)) { *out = v->num_val; return 0; }
    if (ws2_is_bool(v)) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (!ws2_is_str(v)) {
        char msg[256];
        snprintf(msg, sizeof msg, "float() argument must be a string or a real number, not '%s'",
                 ws2_type_name(v));
        *err = ws2_raise("TypeError", msg);
        return -1;
    }
    const char *raw = v->str_val ? v->str_val : "";
    char *s = ws2_strip(raw);
    const char *p = s;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    /* inf / infinity / nan (case-insensitive, full consumption) */
    char low[32]; size_t li = 0;
    for (const char *q = p; *q && li < 31; q++) low[li++] = (char)((*q >= 'A' && *q <= 'Z') ? *q + 32 : *q);
    low[li] = '\0';
    if (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0) {
        *out = neg ? -INFINITY : INFINITY; free(s); return 0;
    }
    if (strcmp(low, "nan") == 0) { *out = NAN; free(s); return 0; }
    char clean[512]; size_t cl = 0;
    if (neg) clean[cl++] = '-'; /* the numeric path must carry the sign */
    int saw_digit = 0, saw_dot = 0;
    long n = ws2_digit_run(p, clean, &cl);
    if (n < 0) goto bad;
    if (n > 0) saw_digit = 1;
    p += n;
    if (*p == '.') {
        saw_dot = 1;
        clean[cl++] = '.';
        p++;
        n = ws2_digit_run(p, clean, &cl);
        if (n < 0) goto bad;
        if (n > 0) saw_digit = 1;
        p += n;
    }
    (void)saw_dot;
    if (!saw_digit) goto bad;
    if (*p == 'e' || *p == 'E') {
        clean[cl++] = 'e';
        p++;
        if (*p == '+' || *p == '-') { clean[cl++] = *p; p++; }
        n = ws2_digit_run(p, clean, &cl);
        if (n <= 0) goto bad;
        p += n;
    }
    if (*p != '\0') goto bad;
    clean[cl] = '\0';
    *out = strtod(clean, NULL);
    free(s);
    return 0;
bad:;
    char *r = ws2_str_repr(raw);
    char msg[600];
    snprintf(msg, sizeof msg, "could not convert string to float: %s", r);
    free(r); free(s);
    *err = ws2_raise("ValueError", msg);
    return -1;
}

/* Python int(value) — truncation toward zero for reals, base-10 string
 * parse. Result rides a double (fixtures stay inside 2^53). */
static int ws2_int_of(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (ws2_is_bool(v)) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (ws2_is_num(v)) { *out = trunc(v->num_val); return 0; }
    if (!ws2_is_str(v)) {
        char msg[256];
        snprintf(msg, sizeof msg,
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 ws2_type_name(v));
        *err = ws2_raise("TypeError", msg);
        return -1;
    }
    const char *raw = v->str_val ? v->str_val : "";
    char *s = ws2_strip(raw);
    const char *p = s;
    char clean[512]; size_t cl = 0;
    if (*p == '+' || *p == '-') { clean[cl++] = *p; p++; }
    long n = ws2_digit_run(p, clean, &cl);
    if (n <= 0 || p[n] != '\0') {
        char *r = ws2_str_repr(raw);
        char msg[600];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", r);
        free(r); free(s);
        *err = ws2_raise("ValueError", msg);
        return -1;
    }
    clean[cl] = '\0';
    *out = strtod(clean, NULL);
    free(s);
    return 0;
}

/* list()/for-loop iteration items: array -> elements, dict -> keys,
 * str -> codepoint chars; anything else is a Python TypeError. */
static json_t *ws2_iter_items(const json_t *v, json_t **err) {
    *err = NULL;
    if (ws2_is_arr(v)) return json_copy(v);
    json_t *out = json_array();
    if (ws2_is_obj(v)) {
        for (size_t i = 0; i < v->c.count; i++)
            json_append(out, json_string(v->c.keys[i]));
        return out;
    }
    if (ws2_is_str(v)) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p) {
            const char *nx = p;
            ws2_cp_next(&nx);
            size_t n = (size_t)(nx - p);
            char *c = malloc(n + 1);
            if (c) { memcpy(c, p, n); c[n] = '\0'; json_append(out, json_string(c)); free(c); }
            p = nx;
        }
        return out;
    }
    json_free(out);
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object is not iterable", ws2_type_name(v));
    *err = ws2_raise("TypeError", msg);
    return NULL;
}

/* list(x or []) — falsy collapses to the empty list first. */
static json_t *ws2_list_or_empty(const json_t *v, json_t **err) {
    if (!ws2_truthy(v)) return json_array();
    return ws2_iter_items(v, err);
}
/* ── faithful static ports of unclaimed sibling helpers (no PoP credit) ── */

/* web_server_memory._coerce_bool(value, *, default) */
static int ws2_coerce_bool(const json_t *value, int default_val, json_t **err) {
    *err = NULL;
    if (ws2_is_bool(value)) return value->bool_val ? 1 : 0;
    if (ws2_none_or_empty(value)) return default_val;
    if (ws2_is_num(value)) return value->num_val != 0.0 ? 1 : 0;
    char *s = ws2_pystr(value);
    char *t = ws2_strip(s);
    char *l = ws2_lower(t);
    free(s); free(t);
    int r = -1;
    if (!strcmp(l, "1") || !strcmp(l, "true") || !strcmp(l, "yes") || !strcmp(l, "on")) r = 1;
    else if (!strcmp(l, "0") || !strcmp(l, "false") || !strcmp(l, "no") || !strcmp(l, "off")) r = 0;
    free(l);
    if (r < 0) {
        char *vs = ws2_pystr(value);
        char msg[600];
        snprintf(msg, sizeof msg, "Invalid boolean value: %s", vs);
        free(vs);
        *err = ws2_raise("ValueError", msg);
        return 0;
    }
    return r;
}

/* web_server_memory._field_default(field) — new node or raise spec in *err. */
static json_t *ws2_field_default(const json_t *field, json_t **err) {
    *err = NULL;
    json_t *kind = json_obj_get(field, "kind");
    if (!kind) { *err = ws2_key_error_str("kind"); return NULL; }
    json_t *d = json_obj_get(field, "default");
    json_t *def = d ? json_copy(d) : json_string("");
    if (ws2_is_str(kind) && strcmp(kind->str_val, "boolean") == 0) {
        int b = ws2_coerce_bool(def, 0, err);
        json_free(def);
        if (*err) return NULL;
        return json_bool(b != 0);
    }
    return def;
}

/* {opt["value"] for opt in field.get("options", [])} membership for a
 * string value. The comprehension builds the WHOLE set before the test,
 * so a bad option raises even when an earlier option matched. */
static int ws2_select_contains(const json_t *field, const char *value, json_t **err) {
    *err = NULL;
    json_t *options = json_obj_get(field, "options");
    json_t *empty = options ? NULL : json_array();
    json_t *items = ws2_iter_items(options ? options : empty, err);
    json_free(empty);
    if (!items) return 0;
    json_t *vals = json_array();
    for (size_t i = 0; i < items->c.count && !*err; i++) {
        json_t *opt = items->c.items[i];
        if (ws2_is_obj(opt)) {
            json_t *v = json_obj_get(opt, "value");
            if (!v) { *err = ws2_key_error_str("value"); break; }
            json_append(vals, json_copy(v));
        } else if (ws2_is_str(opt)) {
            *err = ws2_raise("TypeError", "string indices must be integers, not 'str'");
        } else if (ws2_is_arr(opt)) {
            *err = ws2_raise("TypeError", "list indices must be integers or slices, not str");
        } else {
            char msg[256];
            snprintf(msg, sizeof msg, "'%s' object is not subscriptable", ws2_type_name(opt));
            *err = ws2_raise("TypeError", msg);
        }
    }
    json_free(items);
    int found = 0;
    if (!*err) {
        json_t *needle = json_string(value);
        for (size_t i = 0; i < vals->c.count; i++)
            if (ws2_eq(vals->c.items[i], needle)) { found = 1; break; }
        json_free(needle);
    }
    json_free(vals);
    return found;
}

/* memory_providers._coerce_schema_number(field, raw) */
static json_t *ws2_coerce_schema_number(const json_t *field, const json_t *raw) {
    json_t *err = NULL;
    json_t *owned = NULL;
    const json_t *value;
    if (!ws2_is_none(raw) && !ws2_none_or_empty(raw)) {
        value = raw;
    } else {
        owned = ws2_field_default(field, &err);
        if (err) return err;
        value = owned;
    }
    json_t *kind = json_obj_get(field, "kind");
    int is_int_kind = ws2_is_str(kind) && strcmp(kind->str_val, "integer") == 0;
    double result = 0.0;
    int failed = 0;
    if (ws2_is_bool(value)) failed = 1;
    else if (ws2_float_of(value, &result, &err)) { json_free(err); err = NULL; failed = 1; }
    else if (!isfinite(result)) failed = 1;
    else if (is_int_kind) {
        if (!ws2_is_integral(result)) failed = 1;
        else result = trunc(result);
    }
    if (failed) {
        json_free(owned);
        json_t *key = json_obj_get(field, "key");
        if (!key) return ws2_key_error_str("key");
        char msg[512];
        snprintf(msg, sizeof msg, "Invalid numeric value for '%s'", key->str_val ? key->str_val : "");
        return ws2_raise("ValueError", msg);
    }
    /* min/max checks sit OUTSIDE the live try — a non-numeric bound is a
     * propagating TypeError, and the message names the RESULT's type. */
    const char *rty = is_int_kind ? "int" : "float";
    json_t *minimum = json_obj_get(field, "minimum");
    json_t *maximum = json_obj_get(field, "maximum");
    json_t *key = json_obj_get(field, "key");
    if (!ws2_is_none(minimum)) {
        if (!ws2_is_num(minimum) && !ws2_is_bool(minimum)) {
            json_free(owned);
            char msg[256];
            snprintf(msg, sizeof msg, "'<' not supported between instances of '%s' and '%s'",
                     rty, ws2_type_name(minimum));
            return ws2_raise("TypeError", msg);
        }
        double mn = ws2_is_bool(minimum) ? (minimum->bool_val ? 1.0 : 0.0) : minimum->num_val;
        if (result < mn) {
            json_free(owned);
            if (!key) return ws2_key_error_str("key");
            char *ms = ws2_pystr(minimum);
            char msg[600];
            snprintf(msg, sizeof msg, "'%s' must be at least %s", key->str_val ? key->str_val : "", ms);
            free(ms);
            return ws2_raise("ValueError", msg);
        }
    }
    if (!ws2_is_none(maximum)) {
        if (!ws2_is_num(maximum) && !ws2_is_bool(maximum)) {
            json_free(owned);
            char msg[256];
            snprintf(msg, sizeof msg, "'>' not supported between instances of '%s' and '%s'",
                     rty, ws2_type_name(maximum));
            return ws2_raise("TypeError", msg);
        }
        double mx = ws2_is_bool(maximum) ? (maximum->bool_val ? 1.0 : 0.0) : maximum->num_val;
        if (result > mx) {
            json_free(owned);
            if (!key) return ws2_key_error_str("key");
            char *ms = ws2_pystr(maximum);
            char msg[600];
            snprintf(msg, sizeof msg, "'%s' must be at most %s", key->str_val ? key->str_val : "", ms);
            free(ms);
            return ws2_raise("ValueError", msg);
        }
    }
    json_free(owned);
    return json_number(result);
}

/* memory_providers._memory_section(config) — borrowed dict, creating or
 * replacing a non-dict config["memory"] in place. */
static json_t *ws2_memory_section(json_t *cfg) {
    json_t *m = json_obj_get(cfg, "memory");
    if (!ws2_is_obj(m)) {
        m = json_object();
        json_set(cfg, "memory", m); /* adopts; frees any old value */
    }
    return m;
}

/* messaging._first_str(candidate, keys) — malloc'd first non-empty or NULL. */
static char *ws2_first_str(const json_t *candidate, const char *const *keys, size_t nkeys) {
    if (!ws2_is_obj(candidate)) return NULL;
    for (size_t i = 0; i < nkeys; i++) {
        json_t *v = json_obj_get(candidate, keys[i]);
        char *s = ws2_truthy(v) ? ws2_pystr(v) : ws2_strdup("");
        char *t = ws2_strip(s);
        free(s);
        if (t && t[0]) return t;
        free(t);
    }
    return NULL;
}

/* messaging._whatsapp_phone_from_identifier(value) — ASCII \d filter
 * (fixtures stay inside ASCII digits). malloc'd or NULL. */
static char *ws2_phone_from_identifier(const json_t *value) {
    char *s = ws2_truthy(value) ? ws2_pystr(value) : ws2_strdup("");
    char *raw = ws2_strip(s);
    free(s);
    if (!raw || !raw[0]) { free(raw); return NULL; }
    char *part = ws2_strdup(raw);
    free(raw);
    char *at = strchr(part, '@');
    if (at) *at = '\0';
    char *colon = strchr(part, ':');
    if (colon) *colon = '\0';
    size_t n = strlen(part);
    char *digits = malloc(n + 1);
    size_t dl = 0;
    if (digits) {
        for (size_t i = 0; i < n; i++)
            if (ws2_ascii_digit((unsigned char)part[i])) digits[dl++] = part[i];
        digits[dl] = '\0';
    }
    free(part);
    if (digits && !digits[0]) { free(digits); return NULL; }
    return digits;
}

/* web_server_profiles._token_volume(row) */
static double ws2_token_volume(const json_t *row, json_t **err) {
    *err = NULL;
    if (!ws2_is_obj(row)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(row));
        *err = ws2_raise("AttributeError", msg);
        return 0.0;
    }
    json_t *zero = json_number(0);
    json_t *a = json_obj_get(row, "input_tokens");
    json_t *b = json_obj_get(row, "output_tokens");
    double r = ws2_num_add(ws2_truthy(a) ? a : zero, ws2_truthy(b) ? b : zero, "+", err);
    json_free(zero);
    return r;
}

/* sorted(rows, key=_token_volume, reverse=True) — stable descending.
 * Keys are computed once per row in list order (a raising key propagates
 * before any reordering). Rows are borrowed pointers reordered in place. */
static json_t *ws2_sort_by_volume(json_t **rows, size_t n) {
    if (n == 0) return NULL;
    double *keys = malloc(n * sizeof(double));
    if (!keys) return NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *err = NULL;
        keys[i] = ws2_token_volume(rows[i], &err);
        if (err) { free(keys); return err; }
    }
    for (size_t i = 1; i < n; i++) {
        json_t *r = rows[i];
        double k = keys[i];
        size_t j = i;
        while (j > 0 && keys[j - 1] < k) {
            rows[j] = rows[j - 1];
            keys[j] = keys[j - 1];
            j--;
        }
        rows[j] = r;
        keys[j] = k;
    }
    free(keys);
    return NULL;
}

/* web_server_memory._field_value(field, data) — shared core of the
 * ws_u_field_value port and the ws_u_field_visible port's dependency. */
static json_t *ws2_field_value_core(const json_t *field, const json_t *data) {
    json_t *kind = json_obj_get(field, "kind");
    if (!kind) return ws2_key_error_str("kind");
    if (ws2_is_str(kind) && strcmp(kind->str_val, "secret") == 0) return json_string("");
    json_t *key = json_obj_get(field, "key");
    if (!key) return ws2_key_error_str("key");
    if (!ws2_is_obj(data)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(data));
        return ws2_raise("AttributeError", msg);
    }
    const json_t *value = json_obj_get(data, key->str_val ? key->str_val : "");
    json_t *owned = NULL;
    if (ws2_none_or_empty(value)) {
        json_t *args = json_object();
        json_t *ek = json_obj_get(field, "_env_key");
        json_set(args, "env_key", ek ? json_copy(ek) : json_null());
        json_t *r = ws2_seam("env_lookup", args);
        if (ws2_is_raise(r)) return ws2_raise_out(r);
        owned = r;
        value = r;
    }
    if (ws2_none_or_empty(value)) {
        json_t *err = NULL;
        json_t *d = ws2_field_default(field, &err);
        if (err) { json_free(owned); return err; }
        json_free(owned);
        owned = d;
        value = d;
    }
    if (ws2_is_str(kind) && strcmp(kind->str_val, "select") == 0) {
        char *vs = ws2_pystr(value);
        json_t *err = NULL;
        int ok = ws2_select_contains(field, vs, &err);
        if (err) { free(vs); json_free(owned); return err; }
        if (ok) {
            free(vs);
            if (owned) return owned;
            return json_copy(value);
        }
        free(vs);
        json_free(owned);
        json_t *d = ws2_field_default(field, &err);
        if (err) return err;
        char *ds = ws2_pystr(d);
        json_free(d);
        json_t *r = json_string(ds);
        free(ds);
        return r;
    }
    if (ws2_is_str(kind) && strcmp(kind->str_val, "boolean") == 0) {
        json_t *err = NULL;
        json_t *d = ws2_field_default(field, &err);
        if (err) { json_free(owned); return err; }
        int defb = ws2_coerce_bool(d, 0, &err);
        json_free(d);
        if (err) { json_free(owned); return err; }
        int b = ws2_coerce_bool(value, defb, &err);
        json_free(owned);
        if (err) return err;
        return json_bool(b != 0);
    }
    char *vs = ws2_pystr(value);
    json_free(owned);
    json_t *r = json_string(vs);
    free(vs);
    return r;
}

/* SENTENCE_BOUNDARY_RE.split — r"(?<=[.!?])(?:\s|\n)|(?:\n\n)".
 * Leftmost-first alternation: after .!? a single whitespace char wins
 * over a \n\n pair at the same position. Fragments may be empty. */
static json_t *ws2_sentence_split(const char *text) {
    json_t *frags = json_array();
    if (!text) text = "";
    const char *p = text;
    const char *frag_start = text;
    unsigned prev = 0;
    size_t i = 0;
    while (*p) {
        const char *nx = p;
        unsigned cp = ws2_cp_next(&nx);
        size_t match = 0;
        if (i > 0 && (prev == '.' || prev == '!' || prev == '?') && ws2_isspace_cp(cp))
            match = 1;
        else if (cp == '\n' && nx[0] == '\n')
            match = 2;
        if (match) {
            size_t n = (size_t)(p - frag_start);
            char *f = malloc(n + 1);
            if (f) { memcpy(f, frag_start, n); f[n] = '\0'; json_append(frags, json_string(f)); free(f); }
            p = nx;
            if (match == 2) p++; /* second \n is one byte */
            frag_start = p;
            prev = (match == 1) ? cp : '\n';
            i += match;
        } else {
            prev = cp;
            p = nx;
            i++;
        }
    }
    size_t n = strlen(frag_start);
    char *f = malloc(n + 1);
    if (f) { memcpy(f, frag_start, n + 1); json_append(frags, json_string(f)); free(f); }
    return frags;
}
/* ── small ordered map (borrowed values; caller manages ownership) ── */

typedef struct {
    char **keys;
    json_t **vals;
    size_t count, cap;
} ws2_map_t;

static void ws2_map_init(ws2_map_t *m) { memset(m, 0, sizeof *m); }

static json_t *ws2_map_get(const ws2_map_t *m, const char *key) {
    for (size_t i = 0; i < m->count; i++)
        if (strcmp(m->keys[i], key) == 0) return m->vals[i];
    return NULL;
}

/* Insert or replace (no value free — ownership stays with the caller). */
static void ws2_map_set(ws2_map_t *m, const char *key, json_t *val) {
    for (size_t i = 0; i < m->count; i++) {
        if (strcmp(m->keys[i], key) == 0) { m->vals[i] = val; return; }
    }
    if (m->count == m->cap) {
        size_t nc = m->cap ? m->cap * 2 : 8;
        char **nk = realloc(m->keys, nc * sizeof(char *));
        if (!nk) return;
        m->keys = nk;
        json_t **nv = realloc(m->vals, nc * sizeof(json_t *));
        if (!nv) return;
        m->vals = nv;
        m->cap = nc;
    }
    m->keys[m->count] = ws2_strdup(key);
    m->vals[m->count] = val;
    m->count++;
}

static void ws2_map_free_keys(ws2_map_t *m) {
    for (size_t i = 0; i < m->count; i++) free(m->keys[i]);
    free(m->keys); free(m->vals);
    memset(m, 0, sizeof *m);
}

/* field["kind"] on a non-dict — Python subscript TypeError shapes. */
static json_t *ws2_subscript_error(const json_t *v) {
    if (ws2_is_str(v)) return ws2_raise("TypeError", "string indices must be integers, not 'str'");
    if (ws2_is_arr(v)) return ws2_raise("TypeError", "list indices must be integers or slices, not str");
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object is not subscriptable", ws2_type_name(v));
    return ws2_raise("TypeError", msg);
}

static const char *const WS2_AUX_COUNTERS[] = {
    "input_tokens", "output_tokens", "estimated_cost", "api_calls"
};

/* ── public ports (original cfn names and signatures; PoP tags moved) ── */

/* PoP: ws_u_validate_reference_timeout @ hermes_cli/web_models.py:_validate_reference_timeout */
json_t *ws_u_validate_reference_timeout(json_t *req) {
    json_t *value = json_obj_get(req, "value");
    if (ws2_none_or_empty(value)) return NULL;
    double timeout;
    if (ws2_is_bool(value)) {
        timeout = NAN;
    } else {
        json_t *err = NULL;
        if (ws2_float_of(value, &timeout, &err)) {
            json_free(err); /* live re-raises a fixed ValueError from exc */
            return ws2_raise("ValueError", "reference_timeout must be a finite positive number");
        }
    }
    if (!isfinite(timeout) || timeout <= 0)
        return ws2_raise("ValueError", "reference_timeout must be a finite positive number");
    return json_number(timeout);
}

/* PoP: ws_u_is_command_provider_block @ hermes_cli/web_server_config.py:_is_command_provider_block */
json_t *ws_u_is_command_provider_block(json_t *req) {
    json_t *value = json_obj_get(req, "value");
    if (!ws2_is_obj(value)) return json_bool(false);
    json_t *t = json_obj_get(value, "type");
    char *s = ws2_truthy(t) ? ws2_pystr(t) : ws2_strdup("");
    char *st = ws2_strip(s);
    free(s);
    char *ptype = ws2_lower(st);
    free(st);
    int reject = ptype[0] && strcmp(ptype, "command") != 0;
    free(ptype);
    if (reject) return json_bool(false);
    json_t *command = json_obj_get(value, "command");
    int ok = 0;
    if (ws2_is_str(command)) {
        char *cs = ws2_strip(command->str_val);
        ok = cs && cs[0];
        free(cs);
    }
    return json_bool(ok != 0);
}

/* PoP: ws_u_models_from_custom_endpoint_entry @ hermes_cli/web_routers/config_env.py:_models_from_custom_endpoint_entry */
json_t *ws_u_models_from_custom_endpoint_entry(json_t *req) {
    json_t *entry = json_obj_get(req, "entry");
    if (!ws2_is_obj(entry)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(entry));
        return ws2_raise("AttributeError", msg);
    }
    json_t *models = json_array();
    json_t *raw = json_obj_get(entry, "models");
    if (ws2_is_obj(raw) || ws2_is_arr(raw)) {
        json_t *err = NULL;
        json_t *items = ws2_iter_items(raw, &err); /* dict iterates KEYS */
        if (err) { json_free(models); return err; }
        for (size_t i = 0; i < items->c.count; i++) {
            char *s = ws2_pystr(items->c.items[i]);
            char *t = ws2_strip(s);
            free(s);
            json_append(models, json_string(t ? t : ""));
            free(t);
        }
        json_free(items);
    }
    json_t *m = json_obj_get(entry, "model");
    json_t *dm = json_obj_get(entry, "default_model");
    const json_t *pick = ws2_truthy(m) ? m : (ws2_truthy(dm) ? dm : NULL);
    char *s = pick ? ws2_pystr(pick) : ws2_strdup("");
    char *def = ws2_strip(s);
    free(s);
    if (def && def[0]) {
        json_t *with_def = json_array();
        json_append(with_def, json_string(def));
        for (size_t i = 0; i < models->c.count; i++) {
            json_append(with_def, models->c.items[i]);
            models->c.items[i] = NULL; /* move */
        }
        json_free(models);
        models = with_def;
    }
    free(def);
    json_t *out = json_array();
    json_t *seen = json_array();
    for (size_t i = 0; i < models->c.count; i++) {
        json_t *model = models->c.items[i];
        if (!ws2_truthy(model)) continue;
        int dup = 0;
        for (size_t j = 0; j < seen->c.count; j++)
            if (ws2_eq(seen->c.items[j], model)) { dup = 1; break; }
        if (dup) continue;
        json_append(seen, json_copy(model));
        json_append(out, model);
        models->c.items[i] = NULL; /* move */
    }
    json_free(seen);
    json_free(models);
    return out;
}

/* PoP: ws_u_webhook_route_summary @ hermes_cli/web_routers/ops.py:_webhook_route_summary */
json_t *ws_u_webhook_route_summary(json_t *req) {
    json_t *route = json_obj_get(req, "route");
    if (!ws2_is_obj(route)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(route));
        return ws2_raise("AttributeError", msg);
    }
    json_t *name = json_obj_get(req, "name");
    json_t *base = json_obj_get(req, "base_url");
    json_t *out = json_object();
    char *ns = ws2_pystr(name);
    json_set_hdr(out, "name", ns);
    json_t *d = json_obj_get(route, "description");
    json_set(out, "description", d ? json_copy(d) : json_string(""));
    json_t *err = NULL;
    json_t *events = ws2_list_or_empty(json_obj_get(route, "events"), &err);
    if (err) { free(ns); json_free(out); return err; }
    json_set(out, "events", events);
    json_t *dv = json_obj_get(route, "deliver");
    json_set(out, "deliver", dv ? json_copy(dv) : json_string("log"));
    json_set(out, "deliver_only", json_bool(ws2_truthy(json_obj_get(route, "deliver_only"))));
    json_t *pr = json_obj_get(route, "prompt");
    json_set(out, "prompt", pr ? json_copy(pr) : json_string(""));
    json_t *sc = json_obj_get(route, "script");
    json_set(out, "script", sc ? json_copy(sc) : json_string(""));
    json_t *skills = ws2_list_or_empty(json_obj_get(route, "skills"), &err);
    if (err) { free(ns); json_free(out); return err; }
    json_set(out, "skills", skills);
    json_t *ca = json_obj_get(route, "created_at");
    json_set(out, "created_at", ca ? json_copy(ca) : json_null());
    char *bs = ws2_pystr(base);
    size_t un = strlen(bs) + strlen("/webhooks/") + strlen(ns) + 1;
    char *url = malloc(un);
    if (url) snprintf(url, un, "%s/webhooks/%s", bs, ns);
    json_set_hdr(out, "url", url ? url : "");
    free(url); free(bs);
    json_t *sig = json_obj_get(route, "signature");
    json_set(out, "signature", sig ? json_copy(sig) : json_null());
    json_set(out, "secret_set", json_bool(ws2_truthy(json_obj_get(route, "secret"))));
    /* enabled: route.get("enabled", True) is not False — identity, not ==
     * (0 is not False, so a numeric 0 stays enabled). */
    json_t *en = json_obj_get(route, "enabled");
    int enabled = !en || !(ws2_is_bool(en) && !en->bool_val);
    json_set(out, "enabled", json_bool(enabled));
    free(ns);
    return out;
}

/* PoP: ws_u_field_value @ hermes_cli/web_server_memory.py:_field_value */
json_t *ws_u_field_value(json_t *req) {
    json_t *field = json_obj_get(req, "field");
    json_t *data = json_obj_get(req, "data");
    if (!ws2_is_obj(field)) return ws2_subscript_error(field);
    return ws2_field_value_core(field, data);
}

/* PoP: ws_u_field_visible @ hermes_cli/web_server_memory.py:_field_visible */
json_t *ws_u_field_visible(json_t *req) {
    json_t *field = json_obj_get(req, "field");
    json_t *data = json_obj_get(req, "data");
    json_t *fbk = json_obj_get(req, "fields_by_key");
    if (!ws2_is_obj(field)) return ws2_subscript_error(field);
    json_t *when = json_obj_get(field, "when");
    if (!ws2_is_obj(when) || when->c.count == 0) return json_bool(true);
    if (ws2_truthy(fbk) && !ws2_is_obj(fbk)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(fbk));
        return ws2_raise("AttributeError", msg);
    }
    for (size_t i = 0; i < when->c.count; i++) {
        const char *dep_key = when->c.keys[i];
        json_t *expected = when->c.items[i];
        json_t *dep_field = ws2_truthy(fbk) ? json_obj_get(fbk, dep_key) : NULL;
        int fallback = !ws2_truthy(dep_field);
        json_t *df;
        if (fallback) {
            df = json_object();
            json_set_hdr(df, "key", dep_key);
            json_set_hdr(df, "kind", "text");
            json_set_hdr(df, "default", "");
            json_set(df, "_env_key", json_null());
        } else {
            df = dep_field;
        }
        json_t *fv = ws2_field_value_core(df, data);
        if (fallback) json_free(df);
        if (ws2_is_raise(fv)) return ws2_raise_out(fv);
        char *a = ws2_pystr(fv);
        json_free(fv);
        char *b = ws2_pystr(expected);
        int neq = strcmp(a ? a : "", b ? b : "") != 0;
        free(a); free(b);
        if (neq) return json_bool(false);
    }
    return json_bool(true);
}

/* PoP: ws_u_coerce_schema_field @ hermes_cli/web_routers/memory_providers.py:_coerce_schema_field */
json_t *ws_u_coerce_schema_field(json_t *req) {
    json_t *field = json_obj_get(req, "field");
    json_t *raw = json_obj_get(req, "raw");
    if (!ws2_is_obj(field)) return ws2_subscript_error(field);
    json_t *kind = json_obj_get(field, "kind");
    if (!kind) return ws2_key_error_str("kind");
    const char *k = ws2_is_str(kind) ? kind->str_val : "";
    if (strcmp(k, "boolean") == 0) {
        json_t *err = NULL;
        json_t *d = ws2_field_default(field, &err);
        if (err) return err;
        int defb = ws2_coerce_bool(d, 0, &err);
        json_free(d);
        if (err) return err;
        int b = ws2_coerce_bool(raw, defb, &err);
        if (err) return err;
        return json_bool(b != 0);
    }
    if (strcmp(k, "integer") == 0 || strcmp(k, "number") == 0)
        return ws2_coerce_schema_number(field, raw);
    char *s = ws2_is_none(raw) ? ws2_strdup("") : ws2_pystr(raw);
    char *value = ws2_strip(s);
    free(s);
    if (strcmp(k, "select") == 0) {
        if (!value[0]) {
            json_t *err = NULL;
            json_t *d = ws2_field_default(field, &err);
            if (err) { free(value); return err; }
            char *ds = ws2_pystr(d);
            json_free(d);
            free(value);
            value = ds;
        }
        json_t *err = NULL;
        int ok = ws2_select_contains(field, value, &err);
        if (err) { free(value); return err; }
        if (!ok) {
            json_t *key = json_obj_get(field, "key");
            if (!key) { free(value); return ws2_key_error_str("key"); }
            char msg[512];
            snprintf(msg, sizeof msg, "Invalid value for '%s'", key->str_val ? key->str_val : "");
            free(value);
            return ws2_raise("ValueError", msg);
        }
        json_t *r = json_string(value);
        free(value);
        return r;
    }
    if (value[0]) {
        json_t *r = json_string(value);
        free(value);
        return r;
    }
    free(value);
    json_t *err = NULL;
    json_t *d = ws2_field_default(field, &err);
    if (err) return err;
    return d;
}

/* PoP: ws_u_aux_task_summary @ hermes_cli/web_server_profiles.py:_aux_task_summary */
json_t *ws_u_aux_task_summary(json_t *req) {
    json_t *err = NULL;
    json_t *items = ws2_iter_items(json_obj_get(req, "aux_rows"), &err);
    if (err) return err;
    ws2_map_t by_task;
    ws2_map_init(&by_task);
    json_t *result = NULL;
    for (size_t i = 0; i < items->c.count && !result; i++) {
        json_t *aux = items->c.items[i];
        if (!ws2_is_obj(aux)) {
            char msg[256];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(aux));
            result = ws2_raise("AttributeError", msg);
            break;
        }
        json_t *task = json_obj_get(aux, "task");
        /* task = aux.get("task") or "" — the RAW value is the dict key, so
         * int 5 and str "5" are distinct tasks (and the row keeps the
         * original type). Unhashable truthy tasks raise live. */
        const json_t *tval = ws2_truthy(task) ? task : NULL;
        char *tsk = tval ? ws2_dict_key(tval) : ws2_strdup("s:");
        if (!tsk) {
            char msg[64];
            snprintf(msg, sizeof msg, "unhashable type: '%s'", ws2_type_name(task));
            result = ws2_raise("TypeError", msg);
            break;
        }
        json_t *d = ws2_map_get(&by_task, tsk);
        if (!d) {
            d = json_object();
            json_set(d, "task", tval ? json_copy(tval) : json_string(""));
            json_set(d, "input_tokens", json_number(0));
            json_set(d, "output_tokens", json_number(0));
            json_set(d, "estimated_cost", json_number(0));
            json_set(d, "api_calls", json_number(0));
            json_set(d, "models", json_array());
            ws2_map_set(&by_task, tsk, d);
        }
        free(tsk);
        for (size_t c = 0; c < 4 && !result; c++) {
            const char *key = WS2_AUX_COUNTERS[c];
            json_t *cur = json_obj_get(d, key);
            json_t *add = json_obj_get(aux, key);
            json_t *zero = json_number(0);
            json_t *aerr = NULL;
            /* d[key] += aux.get(key) or 0 — augmented assignment: the live
             * TypeError names += , and str+str concatenates. */
            json_t *sum = ws2_add(cur, ws2_truthy(add) ? add : zero, "+=", &aerr);
            json_free(zero);
            if (aerr) { result = aerr; break; }
            json_set(d, key, sum);
        }
        if (result) break;
        json_t *model = json_obj_get(aux, "model");
        json_t *unknown = json_string("unknown");
        const json_t *mv = ws2_truthy(model) ? model : unknown;
        json_t *models = json_obj_get(d, "models");
        int found = 0;
        for (size_t mi = 0; mi < models->c.count; mi++)
            if (ws2_eq(models->c.items[mi], mv)) { found = 1; break; }
        if (!found) json_append(models, json_copy(mv));
        json_free(unknown);
    }
    if (!result) {
        size_t n = by_task.count;
        json_t **rows = malloc((n ? n : 1) * sizeof(json_t *));
        for (size_t i = 0; i < n; i++) rows[i] = by_task.vals[i];
        result = ws2_sort_by_volume(rows, n);
        if (!result) {
            result = json_array();
            for (size_t i = 0; i < n; i++) json_append(result, json_copy(rows[i]));
        }
        free(rows);
    }
    for (size_t i = 0; i < by_task.count; i++) json_free(by_task.vals[i]);
    ws2_map_free_keys(&by_task);
    json_free(items);
    return result;
}

/* PoP: ws_u_merge_aux_into_by_model @ hermes_cli/web_server_profiles.py:_merge_aux_into_by_model */
json_t *ws_u_merge_aux_into_by_model(json_t *req) {
    json_t *by_model = json_obj_get(req, "by_model");
    json_t *aux_rows = json_obj_get(req, "aux_rows");
    if (!ws2_truthy(aux_rows)) return by_model ? json_copy(by_model) : json_null();
    json_t *err = NULL;
    json_t *aux_items = ws2_iter_items(aux_rows, &err);
    if (err) return err;
    /* merged maps model key -> the ORIGINAL by_model row (mutated in place,
     * so the post-state echoes the fold) or a fresh aux-only row. */
    ws2_map_t merged;
    ws2_map_init(&merged);
    json_t *fresh = json_array(); /* owns aux-only rows */
    json_t *result = NULL;
    json_t *bm_items = NULL;
    if (ws2_is_arr(by_model)) {
        bm_items = by_model; /* borrowed iteration */
    } else {
        bm_items = ws2_iter_items(by_model, &err);
        if (err) { result = err; goto done; }
    }
    for (size_t i = 0; i < bm_items->c.count && !result; i++) {
        json_t *row = bm_items->c.items[i];
        if (!ws2_is_obj(row)) {
            char msg[256];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(row));
            result = ws2_raise("AttributeError", msg);
            break;
        }
        json_t *m = json_obj_get(row, "model");
        /* Raw-value dict keys (see ws2_dict_key): falsy -> "unknown". */
        char *key = ws2_truthy(m) ? ws2_dict_key(m) : ws2_strdup("s:unknown");
        if (!key) {
            char msg[64];
            snprintf(msg, sizeof msg, "unhashable type: '%s'", ws2_type_name(m));
            result = ws2_raise("TypeError", msg);
            break;
        }
        ws2_map_set(&merged, key, row);
        free(key);
    }
    for (size_t i = 0; i < aux_items->c.count && !result; i++) {
        json_t *aux = aux_items->c.items[i];
        if (!ws2_is_obj(aux)) {
            char msg[256];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(aux));
            result = ws2_raise("AttributeError", msg);
            break;
        }
        json_t *m = json_obj_get(aux, "model");
        char *key = ws2_truthy(m) ? ws2_dict_key(m) : ws2_strdup("s:unknown");
        if (!key) {
            char msg[64];
            snprintf(msg, sizeof msg, "unhashable type: '%s'", ws2_type_name(m));
            result = ws2_raise("TypeError", msg);
            break;
        }
        json_t *target = ws2_map_get(&merged, key);
        if (!target) {
            target = json_object();
            json_set(target, "model", ws2_truthy(m) ? json_copy(m) : json_string("unknown"));
            json_set(target, "input_tokens", json_number(0));
            json_set(target, "output_tokens", json_number(0));
            json_set(target, "estimated_cost", json_number(0));
            json_set(target, "sessions", json_number(0));
            json_set(target, "api_calls", json_number(0));
            ws2_map_set(&merged, key, target);
            json_append(fresh, target); /* fresh array owns it */
        }
        free(key);
        for (size_t c = 0; c < 4 && !result; c++) {
            const char *ck = WS2_AUX_COUNTERS[c];
            json_t *cur = json_obj_get(target, ck);
            json_t *add = json_obj_get(aux, ck);
            json_t *zero = json_number(0);
            json_t *aerr = NULL;
            /* (target.get(key) or 0) + (aux.get(key) or 0) — plain + : a str
             * left operand raises the concatenation TypeError. */
            json_t *sum = ws2_add(ws2_truthy(cur) ? cur : zero,
                                  ws2_truthy(add) ? add : zero, "+", &aerr);
            json_free(zero);
            if (aerr) { result = aerr; break; }
            json_set(target, ck, sum);
        }
        if (result) break;
        json_t *tasks = json_obj_get(target, "aux_tasks");
        if (!json_has(target, "aux_tasks")) {
            tasks = json_array();
            json_set(target, "aux_tasks", tasks);
        }
        if (!ws2_is_arr(tasks)) {
            char msg[256];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'append'", ws2_type_name(tasks));
            result = ws2_raise("AttributeError", msg);
            break;
        }
        json_t *entry = json_object();
        json_t *task = json_obj_get(aux, "task");
        /* {"task": aux.get("task") or "", ...} keeps the raw value type. */
        json_set(entry, "task", ws2_truthy(task) ? json_copy(task) : json_string(""));
        for (size_t c = 0; c < 4; c++) {
            json_t *v = json_obj_get(aux, WS2_AUX_COUNTERS[c]);
            json_set(entry, WS2_AUX_COUNTERS[c], ws2_truthy(v) ? json_copy(v) : json_number(0));
        }
        json_append(tasks, entry);
    }
    if (!result) {
        size_t n = merged.count;
        json_t **rows = malloc((n ? n : 1) * sizeof(json_t *));
        for (size_t i = 0; i < n; i++) rows[i] = merged.vals[i];
        result = ws2_sort_by_volume(rows, n);
        if (!result) {
            result = json_array();
            for (size_t i = 0; i < n; i++) json_append(result, json_copy(rows[i]));
        }
        free(rows);
    }
done:
    if (bm_items && bm_items != by_model) json_free(bm_items);
    json_free(fresh);
    ws2_map_free_keys(&merged);
    json_free(aux_items);
    return result;
}

/* PoP: ws_u_http_response_error_detail @ hermes_cli/web_routers/oauth.py:_http_response_error_detail */
json_t *ws_u_http_response_error_detail(json_t *req) {
    json_t *resp = json_obj_get(req, "resp");
    if (!ws2_is_obj(resp)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'json'", ws2_type_name(resp));
        return ws2_raise("AttributeError", msg);
    }
    json_t *r = ws2_seam("resp.json", json_object());
    json_t *payload = NULL;
    if (ws2_is_raise(r)) {
        if (ws2_is_base_exc(r)) return ws2_raise_out(r);
        json_free(r); /* except Exception: payload = None */
    } else {
        payload = r;
    }
    json_t *result = NULL;
    if (ws2_is_obj(payload)) {
        json_t *error = json_obj_get(payload, "error");
        if (ws2_is_obj(error)) {
            static const char *keys[] = {"message", "error_description", "code", "type"};
            json_t *parts = json_array();
            for (size_t i = 0; i < 4; i++) {
                json_t *v = json_obj_get(error, keys[i]);
                char *s = ws2_pystr(v); /* missing -> "" default; null -> "None" */
                if (!v) { free(s); s = ws2_strdup(""); }
                char *t = ws2_strip(s);
                free(s);
                if (t && t[0]) json_append(parts, json_string(t));
                free(t);
            }
            if (parts->c.count > 0) {
                size_t total = 1;
                for (size_t i = 0; i < parts->c.count; i++)
                    total += strlen(parts->c.items[i]->str_val) + 2;
                char *joined = malloc(total);
                if (joined) {
                    joined[0] = '\0';
                    for (size_t i = 0; i < parts->c.count; i++) {
                        if (i) strcat(joined, ": ");
                        strcat(joined, parts->c.items[i]->str_val);
                    }
                    result = json_string(joined);
                    free(joined);
                }
            }
            json_free(parts);
        }
        if (!result) {
            json_t *cands[4] = {
                error,
                json_obj_get(payload, "detail"),
                json_obj_get(payload, "message"),
                json_obj_get(payload, "error_description"),
            };
            for (size_t i = 0; i < 4 && !result; i++) {
                if (ws2_is_str(cands[i])) {
                    char *t = ws2_strip(cands[i]->str_val);
                    if (t && t[0]) result = json_string(t);
                    free(t);
                }
            }
        }
    }
    if (!result) {
        json_t *text = json_obj_get(resp, "text");
        char *s = ws2_truthy(text) ? ws2_pystr(text) : ws2_strdup("");
        char *t = ws2_strip(s);
        free(s);
        char *sl = ws2_slice(t, 0, 500);
        result = json_string(sl ? sl : "");
        free(sl);
        free(t);
    }
    json_free(payload);
    return result;
}
/* PoP: ws_u_require_memory_provider_ready @ hermes_cli/web_server_memory.py:_require_memory_provider_ready */
json_t *ws_u_require_memory_provider_ready(json_t *req) {
    json_t *name = json_obj_get(req, "name");
    if (!ws2_truthy(name)) return NULL;
    json_t *r = ws2_seam("discover_memory_provider_statuses", json_object());
    if (ws2_is_raise(r)) return ws2_raise_out(r);
    json_t *err = NULL;
    json_t *rows = ws2_iter_items(r, &err);
    json_free(r);
    if (err) return err;
    json_t *found = NULL;
    for (size_t i = 0; i < rows->c.count && !found && !err; i++) {
        json_t *row = rows->c.items[i];
        if (!ws2_is_obj(row)) { err = ws2_subscript_error(row); break; }
        json_t *rn = json_obj_get(row, "name");
        if (!rn) { err = ws2_key_error_str("name"); break; }
        if (ws2_eq(rn, name)) found = row;
    }
    json_t *row_copy = found ? json_copy(found) : NULL;
    json_free(rows);
    if (err) { json_free(row_copy); return err; }
    char *ns = ws2_pystr(name);
    if (!row_copy) {
        char detail[600];
        snprintf(detail, sizeof detail, "Unknown memory provider '%s'.", ns);
        free(ns);
        return ws2_http_exc(400, detail);
    }
    json_t *status = json_obj_get(row_copy, "status");
    if (!status) {
        json_free(row_copy); free(ns);
        return ws2_key_error_str("status");
    }
    if (ws2_is_str(status) && strcmp(status->str_val, "ready") == 0) {
        json_free(row_copy);
        free(ns);
        return NULL;
    }
    if (!ws2_is_str(status)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'replace'", ws2_type_name(status));
        json_free(row_copy);
        free(ns);
        return ws2_raise("AttributeError", msg);
    }
    char *repl = ws2_strdup(status->str_val);
    for (char *p = repl; *p; p++) if (*p == '_') *p = ' ';
    char detail[1024];
    snprintf(detail, sizeof detail,
             "Memory provider '%s' is not ready (%s). Configure it in the dashboard first.", ns, repl);
    free(repl); free(ns);
    json_free(row_copy);
    return ws2_http_exc(400, detail);
}

/* PoP: ws_u_profile_to_dict @ hermes_cli/web_routers/profiles.py:_profile_to_dict */
json_t *ws_u_profile_to_dict(json_t *req) {
    json_t *info = json_obj_get(req, "info");
    /* getattr(info, key, default) — a non-object info (None) has no
     * attributes, so every read falls to the default. */
#define WS2_ATTR(key, defnode) \
    (ws2_is_obj(info) && json_obj_get(info, (key)) ? json_copy(json_obj_get(info, (key))) : (defnode))
    json_t *out = json_object();
    json_t *v;
    v = WS2_ATTR("name", json_string(""));
    json_set(out, "name", v);
    v = WS2_ATTR("path", json_string(""));
    char *ps = ws2_pystr(v);
    json_free(v);
    json_set_hdr(out, "path", ps);
    free(ps);
    v = WS2_ATTR("is_default", json_bool(false));
    json_set(out, "is_default", json_bool(ws2_truthy(v)));
    json_free(v);
    json_set(out, "model", WS2_ATTR("model", json_null()));
    json_set(out, "provider", WS2_ATTR("provider", json_null()));
    v = WS2_ATTR("has_env", json_bool(false));
    json_set(out, "has_env", json_bool(ws2_truthy(v)));
    json_free(v);
    v = WS2_ATTR("skill_count", json_number(0));
    json_t *zero = json_number(0);
    const json_t *sv = ws2_truthy(v) ? v : zero;
    double iv = 0.0;
    json_t *err = NULL;
    if (ws2_int_of(sv, &iv, &err)) {
        json_free(v); json_free(zero); json_free(out);
        return err;
    }
    json_free(v); json_free(zero);
    json_set(out, "skill_count", json_number(iv));
    v = WS2_ATTR("gateway_running", json_bool(false));
    json_set(out, "gateway_running", json_bool(ws2_truthy(v)));
    json_free(v);
    v = WS2_ATTR("description", json_string(""));
    json_set(out, "description", ws2_truthy(v) ? v : json_string(""));
    if (!ws2_truthy(v)) json_free(v);
    v = WS2_ATTR("description_auto", json_bool(false));
    json_set(out, "description_auto", json_bool(ws2_truthy(v)));
    json_free(v);
    v = WS2_ATTR("display_name", json_string(""));
    json_set(out, "display_name", ws2_truthy(v) ? v : json_string(""));
    if (!ws2_truthy(v)) json_free(v);
    v = WS2_ATTR("bot_title", json_string(""));
    json_set(out, "bot_title", ws2_truthy(v) ? v : json_string(""));
    if (!ws2_truthy(v)) json_free(v);
    v = WS2_ATTR("ui_meta", json_object());
    json_set(out, "ui_meta", ws2_truthy(v) ? v : json_object());
    if (!ws2_truthy(v)) json_free(v);
    json_set(out, "distribution_name", WS2_ATTR("distribution_name", json_null()));
    json_set(out, "distribution_version", WS2_ATTR("distribution_version", json_null()));
    json_set(out, "distribution_source", WS2_ATTR("distribution_source", json_null()));
    v = WS2_ATTR("alias_path", json_null());
    json_set(out, "has_alias", json_bool(!ws2_is_none(v)));
    json_free(v);
#undef WS2_ATTR
    return out;
}

/* PoP: ws_u_managed_files_policy @ hermes_cli/web_server_files.py:_managed_files_policy */
json_t *ws_u_managed_files_policy(json_t *req) {
    json_t *cr = json_obj_get(req, "create_root");
    int create_root = cr ? ws2_truthy(cr) : 1;
    json_t *args = json_object();
    json_set_hdr(args, "key", "HERMES_DASHBOARD_FILES_ROOT");
    json_set_hdr(args, "default", "");
    json_t *r = ws2_seam("environ.get", args);
    if (ws2_is_raise(r)) return ws2_raise_out(r);
    if (!ws2_is_str(r)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", ws2_type_name(r));
        json_free(r);
        return ws2_raise("AttributeError", msg);
    }
    char *raw = ws2_strip(r->str_val);
    json_free(r);
    if (raw[0]) {
        json_t *a = json_object();
        if (create_root) {
            json_set_hdr(a, "raw_path", raw);
            r = ws2_seam("ensure_managed_root", a);
        } else {
            json_set_hdr(a, "path", raw);
            r = ws2_seam("canonical_path", a);
        }
        free(raw);
        if (ws2_is_raise(r)) return ws2_raise_out(r);
        json_t *out = json_object();
        json_set(out, "default_path", r);
        json_set(out, "locked_root", json_copy(r));
        json_set(out, "can_change_path", json_bool(false));
        return out;
    }
    free(raw);
    r = ws2_seam("default_hermes_root_is_opt_data", json_object());
    if (ws2_is_raise(r)) return ws2_raise_out(r);
    if (ws2_truthy(r)) {
        json_free(r);
        json_t *root;
        if (create_root) {
            json_t *a = json_object();
            json_set_hdr(a, "raw_path", "/opt/data");
            root = ws2_seam("ensure_managed_root", a);
        } else {
            root = json_string("/opt/data");
        }
        if (ws2_is_raise(root)) return ws2_raise_out(root);
        json_t *out = json_object();
        json_set(out, "default_path", root);
        json_set(out, "locked_root", json_copy(root));
        json_set(out, "can_change_path", json_bool(false));
        return out;
    }
    json_free(r);
    json_t *home = ws2_seam("path_home", json_object());
    if (ws2_is_raise(home)) return ws2_raise_out(home);
    json_t *a = json_object();
    json_set(a, "path", home); /* moves home into the seam-owned args */
    json_t *root = ws2_seam("canonical_path", a);
    if (ws2_is_raise(root)) return ws2_raise_out(root);
    json_t *out = json_object();
    json_set(out, "default_path", root);
    json_set(out, "locked_root", json_null());
    json_set(out, "can_change_path", json_bool(true));
    return out;
}

/* PoP: ws_u_multiplex_port_binding_conflict @ hermes_cli/web_routers/messaging.py:_multiplex_port_binding_conflict */
json_t *ws_u_multiplex_port_binding_conflict(json_t *req) {
    json_t *pid = json_obj_get(req, "platform_id");
    if (ws2_is_arr(pid) || ws2_is_obj(pid)) {
        char msg[256];
        snprintf(msg, sizeof msg, "cannot use '%s' as a set element (unhashable type: '%s')",
                 ws2_type_name(pid), ws2_type_name(pid));
        return ws2_raise("TypeError", msg);
    }
    if (!ws2_is_str(pid) ||
        (strcmp(pid->str_val, "api_server") != 0 && strcmp(pid->str_val, "webhook") != 0))
        return NULL;
    json_t *rp = json_obj_get(req, "requested_profile");
    char *s = ws2_truthy(rp) ? ws2_pystr(rp) : ws2_strdup("");
    char *requested = ws2_strip(s);
    free(s);
    char *low = ws2_lower(requested);
    int use_active = !requested[0] || strcmp(low, "current") == 0;
    free(low);
    json_t *target;
    if (use_active) {
        target = ws2_seam("get_active_profile_name", json_object());
        free(requested);
        if (ws2_is_raise(target)) return ws2_raise_out(target);
    } else {
        json_t *a = json_object();
        json_set_hdr(a, "profile", requested);
        json_t *r = ws2_seam("resolve_profile_dir", a);
        if (ws2_is_raise(r)) { free(requested); return ws2_raise_out(r); }
        json_free(r); /* the resolved dir is unused; only its raise matters */
        target = json_string(requested);
        free(requested);
    }
    if (ws2_is_str(target) &&
        (strcmp(target->str_val, "default") == 0 || strcmp(target->str_val, "custom") == 0)) {
        json_free(target);
        return NULL;
    }
    json_t *r = ws2_seam("default_gateway_multiplexes", json_object());
    if (ws2_is_raise(r)) { json_free(target); return ws2_raise_out(r); }
    int multiplexes = ws2_truthy(r);
    json_free(r);
    if (!multiplexes) { json_free(target); return NULL; }
    char *ts = ws2_pystr(target);
    char msg[1200];
    snprintf(msg, sizeof msg,
             "Cannot enable '%s' on profile '%s': gateway.multiplex_profiles is on and the "
             "default profile's listener already serves it for every profile at /p/%s/. Configure it "
             "on the default profile instead (disabling or clearing it here is still allowed).",
             pid->str_val, ts, ts);
    free(ts);
    json_free(target);
    return json_string(msg);
}

/* PoP: ws_u_infer_provider_on_model_change @ hermes_cli/web_server_config.py:_infer_provider_on_model_change */
json_t *ws_u_infer_provider_on_model_change(json_t *req) {
    json_t *mv = json_obj_get(req, "model_val");
    char *s = ws2_truthy(mv) ? ws2_pystr(mv) : ws2_strdup("");
    char *name = ws2_strip(s);
    free(s);
    json_t *prev = json_obj_get(req, "prev_provider");
    json_t *ret = NULL;
    if (!name[0]) {
        ret = json_array();
        json_append(ret, json_string(""));
        json_append(ret, json_string(name));
        free(name);
        return ret;
    }
    json_t *mi = json_obj_get(req, "models_import");
    if (mi && !ws2_truthy(mi)) { /* except Exception on the models import */
        ret = json_array();
        json_append(ret, json_string(""));
        json_append(ret, json_string(name));
        free(name);
        return ret;
    }
    json_t *args = json_object();
    json_set_hdr(args, "model", name);
    json_set(args, "prev_provider", prev ? json_copy(prev) : json_null());
    json_t *r = ws2_seam("detect_provider_for_model", args);
    json_t *detected = NULL;
    if (ws2_is_raise(r)) {
        if (ws2_is_base_exc(r)) { free(name); return ws2_raise_out(r); }
        json_free(r); /* except Exception: detected = None */
    } else {
        detected = r;
    }
    if (ws2_truthy(detected)) {
        ret = json_array();
        if (ws2_is_arr(detected)) {
            if (detected->c.count >= 2) {
                json_append(ret, json_copy(detected->c.items[0]));
                json_append(ret, json_copy(detected->c.items[1]));
            } else {
                json_free(ret);
                ret = ws2_raise("IndexError", "tuple index out of range");
            }
        } else if (ws2_is_str(detected)) {
            if (ws2_cplen(detected->str_val) >= 2) {
                char *c0 = ws2_slice(detected->str_val, 0, 1);
                char *c1 = ws2_slice(detected->str_val, 1, 2);
                json_append(ret, json_string(c0));
                json_append(ret, json_string(c1));
                free(c0); free(c1);
            } else {
                json_free(ret);
                ret = ws2_raise("IndexError", "string index out of range");
            }
        } else if (ws2_is_obj(detected)) {
            json_free(ret);
            ret = ws2_raise("KeyError", "0"); /* str(KeyError(0)) == "0" */
        } else {
            json_free(ret);
            char msg[256];
            snprintf(msg, sizeof msg, "'%s' object is not subscriptable", ws2_type_name(detected));
            ret = ws2_raise("TypeError", msg);
        }
        json_free(detected);
        free(name);
        return ret;
    }
    json_free(detected);
    if (strchr(name, '/')) {
        json_t *di = json_obj_get(req, "detect_import");
        if (!di || ws2_truthy(di)) {
            /* try: normalize_provider(prev) in _AGGREGATOR_PROVIDERS; the
             * credentials seam fires only when the current provider is NOT
             * an aggregator (short-circuit and). */
            json_t *a = json_object();
            json_set(a, "provider", prev ? json_copy(prev) : json_null());
            r = ws2_seam("normalize_provider", a);
            if (ws2_is_raise(r)) {
                if (ws2_is_base_exc(r)) { free(name); return ws2_raise_out(r); }
                json_free(r);
            } else {
                int cur_agg = 0;
                int unhashable = ws2_is_arr(r) || ws2_is_obj(r);
                if (ws2_is_str(r)) {
                    const char *v = r->str_val;
                    cur_agg = !strcmp(v, "nous") || !strcmp(v, "openrouter") ||
                              !strcmp(v, "ai-gateway") || !strcmp(v, "kilocode");
                }
                json_free(r);
                if (!unhashable && !cur_agg) {
                    a = json_object();
                    json_set_hdr(a, "provider", "openrouter");
                    json_t *r2 = ws2_seam("provider_has_credentials", a);
                    if (ws2_is_raise(r2)) {
                        if (ws2_is_base_exc(r2)) { free(name); return ws2_raise_out(r2); }
                        json_free(r2);
                    } else if (ws2_truthy(r2)) {
                        json_free(r2);
                        ret = json_array();
                        json_append(ret, json_string("openrouter"));
                        json_append(ret, json_string(name));
                        free(name);
                        return ret;
                    } else {
                        json_free(r2);
                    }
                }
            }
        }
    }
    ret = json_array();
    json_append(ret, json_string(""));
    json_append(ret, json_string(name));
    free(name);
    return ret;
}

/* PoP: ws_u_split_text_for_speak_stream @ hermes_cli/web_server_gateway.py:_split_text_for_speak_stream */
json_t *ws_u_split_text_for_speak_stream(json_t *req) {
    json_t *text = json_obj_get(req, "text");
    if (!ws2_is_str(text)) {
        char msg[256];
        snprintf(msg, sizeof msg, "expected string or bytes-like object, got '%s'", ws2_type_name(text));
        return ws2_raise("TypeError", msg);
    }
    json_t *capn = json_obj_get(req, "cap");
    double cap = 4000.0;
    if (ws2_truthy(capn)) {
        if (!ws2_is_num(capn) && !ws2_is_bool(capn)) {
            char msg[256];
            snprintf(msg, sizeof msg, "'>' not supported between instances of '%s' and 'int'",
                     ws2_type_name(capn));
            return ws2_raise("TypeError", msg);
        }
        double cv = ws2_is_bool(capn) ? (capn->bool_val ? 1.0 : 0.0) : capn->num_val;
        if (cv > 0) cap = cv;
    }
    json_t *frags = ws2_sentence_split(text->str_val);
    json_t *pieces = json_array();
    char *buf = ws2_strdup("");
    json_t *result = NULL;
    for (size_t i = 0; i < frags->c.count && !result; i++) {
        const char *frag = frags->c.items[i]->str_val;
        char *t = ws2_strip(frag);
        int keep = t && t[0];
        free(t);
        if (!keep) continue;
        char *sentence = ws2_strdup(frag);
        while (ws2_cplen(sentence) > cap) {
            if (!ws2_is_integral(cap)) {
                result = ws2_raise("TypeError",
                                   "slice indices must be integers or None or have an __index__ method");
                free(sentence);
                break;
            }
            char *piece = ws2_slice(sentence, 0, (size_t)cap);
            json_append(pieces, json_string(piece));
            free(piece);
            char *rest = ws2_slice(sentence, (size_t)cap, (size_t)-1);
            free(sentence);
            sentence = rest;
        }
        if (result) break;
        if (buf[0] && ws2_cplen(buf) + ws2_cplen(sentence) + 1 > cap) {
            json_append(pieces, json_string(buf));
            free(buf);
            buf = sentence;
        } else if (buf[0]) {
            size_t n = strlen(buf) + 1 + strlen(sentence) + 1;
            char *nb = malloc(n);
            if (nb) snprintf(nb, n, "%s %s", buf, sentence);
            free(buf); free(sentence);
            buf = nb ? nb : ws2_strdup("");
        } else {
            free(buf);
            buf = sentence;
        }
    }
    if (!result && buf[0]) json_append(pieces, json_string(buf));
    free(buf);
    json_free(frags);
    if (result) { json_free(pieces); return result; }
    return pieces;
}

/* PoP: ws_u_get_dashboard_plugins @ hermes_cli/web_server.py:_get_dashboard_plugins */
json_t *ws_u_get_dashboard_plugins(json_t *req) {
    json_t *fr = json_obj_get(req, "force_rescan");
    int force = fr ? ws2_truthy(fr) : 0;
    json_t *home = ws2_seam("secondary_profile_home", json_object());
    if (ws2_is_raise(home)) return ws2_raise_out(home);
    if (!ws2_is_none(home)) {
        json_t *a = json_object();
        json_set(a, "home", home); /* moves home into seam-owned args */
        json_set(a, "force_rescan", json_bool(force != 0));
        json_t *r = ws2_seam("profile_dashboard_plugins", a);
        if (ws2_is_raise(r)) return ws2_raise_out(r);
        return r;
    }
    json_free(home);
    json_t *cache = json_obj_get(req, "cache"); /* borrowed global mirror */
    int stale = ws2_is_none(cache) || force;
    if (!stale) {
        json_t *err = NULL;
        json_t *items = ws2_iter_items(cache, &err);
        if (err) return err;
        for (size_t i = 0; i < items->c.count; i++) {
            json_t *p = items->c.items[i];
            if (!ws2_is_obj(p)) { err = ws2_subscript_error(p); break; }
            json_t *d = json_obj_get(p, "_dir");
            if (!d) { err = ws2_key_error_str("_dir"); break; }
            if (!ws2_is_str(d)) {
                char msg[256];
                snprintf(msg, sizeof msg,
                         "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
                         ws2_type_name(d));
                err = ws2_raise("TypeError", msg);
                break;
            }
            json_t *a = json_object();
            json_set(a, "path", json_copy(d));
            json_t *r = ws2_seam("path_is_dir", a);
            if (ws2_is_raise(r)) { err = ws2_raise_out(r); break; }
            int is_dir = ws2_truthy(r);
            json_free(r);
            if (!is_dir) { stale = 1; break; } /* any() short-circuits */
        }
        json_free(items);
        if (err) return err;
    }
    if (stale) {
        json_t *nc = ws2_seam("discover_dashboard_plugins", json_object());
        if (ws2_is_raise(nc)) return ws2_raise_out(nc);
        json_t *ret = json_copy(nc);
        json_set(req, "cache", nc); /* adopts; old cache node freed here */
        return ret;
    }
    json_t *ret = json_copy(cache);
    json_set(req, "cache", json_copy(cache)); /* keep the state key present */
    return ret;
}

/* PoP: ws_u_update_memory_provider_config @ hermes_cli/web_routers/memory_providers.py:_update_memory_provider_config */
json_t *ws_u_update_memory_provider_config(json_t *req) {
    json_t *provider = json_obj_get(req, "provider");
    json_t *values = json_obj_get(req, "values");
    if (!ws2_is_obj(provider)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'storage'", ws2_type_name(provider));
        return ws2_raise("AttributeError", msg);
    }
    json_t *storage = json_obj_get(provider, "storage");
    json_t *name = json_obj_get(provider, "name");
    if (!storage) return ws2_raise("AttributeError", "'ProviderConfigSchema' object has no attribute 'storage'");
    if (!name) return ws2_raise("AttributeError", "'ProviderConfigSchema' object has no attribute 'name'");
    if (ws2_is_arr(storage) || ws2_is_obj(storage)) {
        char msg[256];
        snprintf(msg, sizeof msg, "cannot use '%s' as a dict key (unhashable type: '%s')",
                 ws2_type_name(storage), ws2_type_name(storage));
        return ws2_raise("TypeError", msg);
    }
    const char *writer = NULL;
    if (ws2_is_str(storage)) {
        if (strcmp(storage->str_val, "flat_json") == 0) writer = "write_provider_flat";
        else if (strcmp(storage->str_val, "provider") == 0) writer = "write_provider_own";
    }
    if (!writer) {
        char *ns = ws2_pystr(name);
        char *sr = ws2_pyrepr(storage);
        char msg[700];
        snprintf(msg, sizeof msg, "Memory provider %s declares unsupported config storage %s", ns, sr);
        free(ns); free(sr);
        return ws2_raise("ValueError", msg);
    }
    json_t *args = json_object();
    json_set(args, "provider", json_copy(provider));
    json_set(args, "values", values ? json_copy(values) : json_null());
    json_t *r = ws2_seam(writer, args);
    if (ws2_is_raise(r)) return ws2_raise_out(r);
    json_free(r);
    json_t *cfg = ws2_seam("load_config", json_object());
    if (ws2_is_raise(cfg)) return ws2_raise_out(cfg);
    if (!ws2_is_obj(cfg)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(cfg));
        json_free(cfg);
        return ws2_raise("AttributeError", msg);
    }
    json_t *mem = ws2_memory_section(cfg);
    json_t *mp = json_obj_get(mem, "provider");
    if (!ws2_eq(mp, name)) {
        json_set(mem, "provider", json_copy(name));
        json_t *a = json_object();
        json_set(a, "config", json_copy(cfg));
        r = ws2_seam("save_config", a);
        if (ws2_is_raise(r)) { json_free(cfg); return ws2_raise_out(r); }
        json_free(r);
    }
    json_set(req, "config", cfg); /* state echo: the loaded+mutated config */
    return NULL;
}

/* PoP: ws_u_whatsapp_linked_account_from_session @ hermes_cli/web_routers/messaging.py:_whatsapp_linked_account_from_session */
json_t *ws_u_whatsapp_linked_account_from_session(json_t *req) {
    json_t *sp = json_obj_get(req, "session_path");
    if (!ws2_is_str(sp)) {
        char msg[256];
        snprintf(msg, sizeof msg,
                 "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
                 ws2_type_name(sp));
        return ws2_raise("TypeError", msg);
    }
    /* Path(session_path) / "creds.json" — pathlib drops trailing slashes
     * and an empty base collapses to the bare child. */
    size_t n = strlen(sp->str_val);
    while (n > 0 && sp->str_val[n - 1] == '/') n--;
    char *path = malloc(n + strlen("/creds.json") + 1);
    if (!path) return NULL;
    memcpy(path, sp->str_val, n);
    strcpy(path + n, n > 0 ? "/creds.json" : "creds.json");
    json_t *args = json_object();
    json_set_hdr(args, "path", path);
    json_set_hdr(args, "encoding", "utf-8-sig");
    free(path);
    json_t *r = ws2_seam("read_text", args);
    json_t *triple = json_array();
    if (ws2_is_raise(r)) {
        if (ws2_is_base_exc(r)) { json_free(triple); return ws2_raise_out(r); }
        json_free(r); /* except Exception: return None, None, None */
        json_append(triple, json_null());
        json_append(triple, json_null());
        json_append(triple, json_null());
        return triple;
    }
    if (!ws2_is_str(r)) {
        /* json.loads(non-str) raises inside the try -> triple None */
        json_free(r);
        json_append(triple, json_null());
        json_append(triple, json_null());
        json_append(triple, json_null());
        return triple;
    }
    json_t *payload = json_parse(r->str_val, NULL);
    json_free(r);
    if (!payload) {
        json_append(triple, json_null());
        json_append(triple, json_null());
        json_append(triple, json_null());
        return triple;
    }
    if (!ws2_is_obj(payload)) {
        /* payload.get sits OUTSIDE the try — AttributeError propagates */
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ws2_type_name(payload));
        json_free(payload);
        json_free(triple);
        return ws2_raise("AttributeError", msg);
    }
    static const char *id_keys[] = {"id", "jid", "lid"};
    static const char *name_keys[] = {"name", "verifiedName", "notify", "pushName"};
    json_t *candidates[3] = {
        json_obj_get(payload, "me"), json_obj_get(payload, "account"), payload
    };
    char *account_id = NULL;
    char *account_name = NULL;
    for (size_t i = 0; i < 3 && !account_id; i++)
        account_id = ws2_first_str(candidates[i], id_keys, 3);
    for (size_t i = 0; i < 3 && !account_name; i++)
        account_name = ws2_first_str(candidates[i], name_keys, 4);
    json_t *idnode = account_id ? json_string(account_id) : NULL;
    char *phone = ws2_phone_from_identifier(idnode);
    json_free(idnode);
    json_append(triple, account_id ? json_string(account_id) : json_null());
    json_append(triple, account_name ? json_string(account_name) : json_null());
    json_append(triple, phone ? json_string(phone) : json_null());
    free(account_id); free(account_name); free(phone);
    json_free(payload);
    return triple;
}
