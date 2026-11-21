/*
 * discord_adapter_flow9.c -- FLOW port of 16 UNCLAIMED defs of
 * plugins/platforms/discord/adapter.py (hermes-agent): recovery / slash
 * event building, task cancellation, media caching and YAML->env bridging.
 *
 * Each carries a PoP annotation at its definition below. None are claimed by
 * committed flow1/flow2/flow5/flow6/flow7/flow8. Decision sequences only:
 * every live I/O / injected callable / claimed sibling enters through ONE
 * universal seam da9_call(ctx, name, args). The harness wires it to a
 * recording stub fed in.io; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. Seam-name set
 * is exactly the SEAM TABLE cross-checked (set equality) by the oracle.
 *
 * Self-contained: libjson only; the harness #includes this .c directly.
 * All symbols are static and da9_-prefixed.
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow9.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - Exception = {"__raise__": cls, "message": str(exc)}; full builtin
 *   hierarchy modeled so BaseException (SystemExit/KeyboardInterrupt/
 *   CancelledError) passes through every except Exception site; missing or
 *   exhausted script = fail-closed RuntimeError on BOTH sides.
 * - Attribute doubles are JSON objects materialized as SimpleNamespace
 *   (ALWAYS truthy). getattr-default reads by KEY PRESENCE; a truthy slot at
 *   a callable key fires that seam.
 * - state carries the adapter attrs each def reads; io carries seam scripts.
 * - str(None)=="None" for pystr_full; int() parses ASCII digits with sign/ws.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *da9_strdup(const char *s); /* forward decl (da9_exc_str uses it) */

/* -- universal seam context -- */
typedef struct da9_ctx {
    json_t *state;      /* owned; input state copy, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da9_ctx_t;

/* -- basic helpers -- */
static bool da9_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *da9_raise_kind(const json_t *v) {
    return da9_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool da9_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool da9_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
/* OBJECT attribute doubles are always truthy (SimpleNamespace semantics). */
static bool da9_attr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_OBJECT) return true;
    return da9_truthy(v);
}
static const char *da9_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool da9_str_eq(const json_t *v, const char *s) {
    const char *t = da9_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *da9_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* -- full builtin exception hierarchy (parent map) -- */
typedef struct { const char *k; const char *p; } da9_exc_row_t;
static const da9_exc_row_t DA8_EXC_PARENTS[] = {
    {"BaseException", NULL},
    {"Exception", "BaseException"},
    {"GeneratorExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BufferError", "Exception"},
    {"EOFError", "Exception"}, {"ExceptionGroup", "Exception"},
    {"ImportError", "Exception"}, {"LookupError", "Exception"},
    {"MemoryError", "Exception"}, {"NameError", "Exception"},
    {"OSError", "Exception"}, {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"}, {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"}, {"SyntaxError", "Exception"},
    {"SystemError", "Exception"}, {"TypeError", "Exception"},
    {"ValueError", "Exception"}, {"Warning", "Exception"},
    {"EnvironmentError", "OSError"}, {"IOError", "OSError"},
    {"FloatingPointError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"}, {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"}, {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"}, {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"BrokenPipeError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
    {"BytesWarning", "Warning"}, {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"}, {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"}, {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"}, {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"CancelledError", "BaseException"},
    {"asyncio.CancelledError", "BaseException"},
};
static const char *da9_exc_parent(const char *k) {
    for (size_t i = 0; i < sizeof(DA8_EXC_PARENTS) / sizeof(DA8_EXC_PARENTS[0]); i++)
        if (strcmp(k, DA8_EXC_PARENTS[i].k) == 0) return DA8_EXC_PARENTS[i].p;
    return NULL;
}
/* True when kind extends target (identity included). Unknown kinds extend
 * Exception, matching a dynamically-created type(kind,(Exception,)). */
static bool da9_exc_extends(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int guard = 0; guard < 32 && cur; guard++) {
        if (strcmp(cur, target) == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false; /* known root, not target */
        const char *p = da9_exc_parent(cur);
        if (!p) {
            /* unknown leaf behaves as an Exception subclass */
            return strcmp(target, "Exception") == 0 || strcmp(target, "BaseException") == 0;
        }
        cur = p;
    }
    return false;
}
static bool da9_is_base_exc(const char *kind) {
    if (!kind) return false;
    if (strcmp(kind, "BaseException") == 0) return true;
    return da9_exc_extends(kind, "BaseException") && !da9_exc_extends(kind, "Exception");
}
static bool da9_is_value_or_type(const char *kind) {
    return da9_exc_extends(kind, "ValueError") || da9_exc_extends(kind, "TypeError");
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc/proj_exc. */
static json_t *da9_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && da9_exc_extends(type, "OSError")) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = da9_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return da9_err(type, msg);
}
/* str(exc) from a raise descriptor is already the projected message. */
static char *da9_exc_str(const json_t *exc) {
    return da9_strdup(json_get_str(exc, "message", ""));
}

/* -- the one universal seam (mirrors oracle Ctx.call exactly) -- */
static json_t *da9_call(da9_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da9_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da9_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = da9_make_exc(raise);
            else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* -- string helpers -- */
static char *da9_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static json_t *da9_snode(const char *s) { return json_string(s ? s : ""); }
static unsigned int da9_decode_cp(const unsigned char *p) {
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xe0) == 0xc0) return ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0)
        return ((unsigned)(p[0] & 0x0f) << 12) | ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3f) << 12)
         | ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}
static size_t da9_cp_len(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xe0) == 0xc0) return 2;
    if ((p[0] & 0xf0) == 0xe0) return 3;
    return 4;
}
static bool da9_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f)) return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
/* str.strip(): full Unicode White_Space, both ends. Caller frees. */
static char *da9_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *p = (const unsigned char *)s;
    while (*p && da9_is_ws_cp(da9_decode_cp(p))) p += da9_cp_len(p);
    const unsigned char *end = p + strlen((const char *)p);
    while (end > p) {
        const unsigned char *q = end - 1;
        while (q > p && (*q & 0xc0) == 0x80) q--;
        if (!da9_is_ws_cp(da9_decode_cp(q))) break;
        end = q;
    }
    size_t n = (size_t)(end - p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}
/* str.lstrip(chars): strip leading single-byte chars present in `chars`. */
static char *da9_lstrip_chars(const char *s, const char *chars) {
    if (!s) s = "";
    const char *p = s;
    while (*p && strchr(chars, *p)) p++;
    return da9_strdup(p);
}
static char *da9_lower(const char *s) {
    char *o = da9_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *da9_first_line(const char *s) {
    const char *nl = strchr(s ? s : "", '\n');
    size_t n = nl ? (size_t)(nl - s) : strlen(s ? s : "");
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}
static size_t da9_cp_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) { n++; p += da9_cp_len(p); }
    return n;
}
static char *da9_prefix(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *start = p;
    size_t k = 0;
    while (*p && k < n) { p += da9_cp_len(p); k++; }
    size_t bytes = (size_t)(p - start);
    char *o = malloc(bytes + 1);
    if (!o) return NULL;
    memcpy(o, start, bytes);
    o[bytes] = '\0';
    return o;
}
static char *da9_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da9_strdup("");
    switch (v->type) {
        case JSON_STRING: return da9_strdup(v->str_val);
        case JSON_BOOL: return da9_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == floor(v->num_val) && isfinite(v->num_val))
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else snprintf(buf, sizeof buf, "%g", v->num_val);
            return da9_strdup(buf);
        }
        default: return da9_strdup("");
    }
}
static char *da9_strval(const json_t *v, const char *dflt) {
    if (da9_is_null(v) || !da9_truthy(v)) return da9_strdup(dflt ? dflt : "");
    return da9_pystr(v);
}
static json_t *da9_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static bool da9_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static json_t *da9_arg_get(const json_t *obj, const char *key) {
    json_t *v = da9_get(obj, key);
    return (v && v->type != JSON_NULL) ? json_copy(v) : json_null();
}
static bool da9_int(const json_t *v, long long *out) {
    if (!v) return false;
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return false;
        *out = (long long)v->num_val;
        return true;
    }
    if (v->type != JSON_STRING) return false;
    const char *s = v->str_val;
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    bool neg = false;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    long long acc = 0;
    int nd = 0;
    for (; *s >= '0' && *s <= '9'; s++) {
        acc = acc * 10 + (*s - '0');
        nd++;
    }
    if (!nd) return false;
    /* Python int() strips trailing whitespace too (e.g. int(' 42 ') == 42). */
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    if (*s != '\0') return false;
    *out = neg ? -acc : acc;
    return true;
}


/* -- extra helpers for flow6 -- */
/* str(None) == "None" (da9_pystr maps NULL to ""; Python str maps it to "None"). */
static char *da9_pystr_full(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da9_strdup("None");
    return da9_pystr(v);
}
/* str.isdigit(): non-empty, all ASCII digits (fixture corpus is ASCII). */
static bool da9_isdigit_str(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}
/* cast a JSON value to a number the way int()/float() does; *ok=false on failure. */
static json_t *da9_cast(const json_t *v, const char *how, bool *ok) {
    *ok = true;
    if (!how) how = "int";
    if (strcmp(how, "int") == 0) {
        long long n;
        if (!da9_int(v, &n)) { *ok = false; return NULL; }
        return json_number((double)n);
    }
    /* float */
    if (da9_is_null(v) || (v && v->type == JSON_BOOL)) { *ok = false; return NULL; }
    if (v && v->type == JSON_NUMBER) return json_copy(v);
    if (!v || v->type != JSON_STRING) { *ok = false; return NULL; }
    char *end = NULL;
    double d = strtod(v->str_val, &end);
    if (!end || *end != '\0') { *ok = false; return NULL; }
    return json_number(d);
}
static bool da9_truthy_set(const char *s) {
    return strcmp(s, "true") == 0 || strcmp(s, "1") == 0 ||
           strcmp(s, "yes") == 0 || strcmp(s, "on") == 0;
}

/* ===================== flow7 extra helpers ===================== */
/* getattr(obj, key, default): KEY PRESENCE (present-null counts as present). */
static json_t *da9_attr(const json_t *obj, const char *key, const json_t *dflt) {
    json_t *v = da9_get(obj, key);
    return v ? json_copy(v) : (dflt ? json_copy(dflt) : json_null());
}
/* direct attribute access that raises AttributeError when the key is absent */
static json_t *da9_need(const json_t *obj, const char *key) {
    json_t *v = da9_get(obj, key);
    if (!v) {
        char msg[256];
        snprintf(msg, sizeof msg, "'SimpleNamespace' object has no attribute '%s'", key);
        return da9_err("AttributeError", msg);
    }
    return json_copy(v);
}
static bool da9_startswith(const char *s, const char *pre) {
    if (!s) return false;
    size_t n = strlen(pre);
    return strncmp(s, pre, n) == 0;
}
/* canonical-set membership over a JSON array (compares str() forms) */
static bool da9_in_set(const json_t *arr, const json_t *v) {
    if (!arr || arr->type != JSON_ARRAY || da9_is_null(v)) return false;
    char *s = da9_pystr_full(v);
    bool hit = false;
    for (size_t i = 0; i < json_len(arr) && !hit; i++) {
        char *t = da9_pystr_full(json_get(arr, i));
        if (strcmp(s, t) == 0) hit = true;
        free(t);
    }
    free(s);
    return hit;
}
/* Python str() of the exception int() would raise for a bad raw value. */
static char *da9_int_err(const json_t *raw) {
    char buf[512];
    if (raw && raw->type == JSON_STRING)
        snprintf(buf, sizeof buf, "invalid literal for int() with base 10: '%s'", raw->str_val);
    else if (da9_is_null(raw))
        snprintf(buf, sizeof buf,
                 "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    else
        snprintf(buf, sizeof buf,
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 (!raw) ? "NoneType" : "SimpleNamespace");
    return da9_strdup(buf);
}
/* truthiness of a callable slot: present and truthy */
static bool da9_callable_slot(const json_t *obj, const char *key) {
    json_t *v = da9_get(obj, key);
    return v && da9_truthy(v);
}


/* ===================== flow9 extra helpers ===================== */
static char *da9_rstrip_chars(const char *s, const char *chars) {
    if (!s) s = "";
    char *o = da9_strdup(s);
    size_t n = strlen(o);
    while (n > 0 && strchr(chars, o[n - 1])) o[--n] = '\0';
    return o;
}
static bool da9_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}
/* substring from codepoint index start to end */
static char *da9_substr_cp(const char *s, size_t start) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    size_t k = 0;
    while (*p && k < start) { p += da9_cp_len(p); k++; }
    return da9_strdup((const char *)p);
}
/* Python: ",".join(str(v) for v in value) if list else str(value). */
static json_t *da9_csv(const json_t *v) {
    char buf[16384]; size_t n = 0;
    buf[0] = '\0';
    if (v && v->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(v); i++) {
            if (n && n < sizeof(buf) - 1) buf[n++] = ',';
            char *s = da9_pystr_full(json_get(v, i));
            size_t l = strlen(s);
            if (n + l >= sizeof(buf)) l = sizeof(buf) - 1 - n;
            memcpy(buf + n, s, l); n += l; free(s);
        }
    } else {
        char *s = da9_pystr_full(v);
        size_t l = strlen(s);
        if (l >= sizeof(buf)) l = sizeof(buf) - 1;
        memcpy(buf, s, l); n = l; free(s);
    }
    buf[n] = '\0';
    return json_string(buf);
}
/* pathlib `a / b` for the simple relative paths used by the fixtures. */
static char *da9_path_join(const char *a, const char *b) {
    if (!a || !*a) {
        const char *t = b ? b : "";
        char *o = malloc(strlen(t) + 2);
        if (o) snprintf(o, strlen(t) + 2, "/%s", t);
        return o;
    }
    size_t la = strlen(a);
    bool slash = a[la - 1] == '/';
    size_t lb = strlen(b ? b : "");
    char *o = malloc(la + lb + 2);
    if (!o) return da9_strdup("");
    snprintf(o, la + lb + 2, "%s%s%s", a, slash ? "" : "/", b ? b : "");
    return o;
}
/* lower-cased pystr_full (ASCII) */
static char *da9_lower_pystr(const json_t *v) { return da9_lower(da9_pystr_full(v)); }
/* last extension via os.path.splitext: returns text after the LAST dot in
 * the basename (including the dot), or "" if none. */
static char *da9_splitext(const char *name) {
    if (!name) return da9_strdup("");
    const char *base = strrchr(name, '/');
    base = base ? base + 1 : name;
    const char *dot = strrchr(base, '.');
    return dot ? da9_strdup(dot) : da9_strdup("");
}


/* ==================== DEF 1: _clean_discord_id ==================== */
/* PoP: da9_clean_discord_id @ plugins/platforms/discord/adapter.py:_clean_discord_id */
static json_t *da9_clean_discord_id(da9_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *s = da9_strip(json_get_str(args, "entry", ""));
    if (da9_startswith(s, "<@") && da9_endswith(s, ">")) {
        char *a = da9_lstrip_chars(s, "<@!");
        char *b = da9_rstrip_chars(a, ">");
        free(s); free(a); s = b;
    }
    char *lw = da9_lower(s);
    if (da9_startswith(lw, "user:")) {
        char *sub = da9_substr_cp(s, 5);
        free(s); s = sub;
    }
    free(lw);
    char *out = da9_strip(s);
    json_t *r = json_string(out ? out : "");
    free(out); free(s);
    return r;
}

/* ==================== DEF 2: _command_sync_state_path ==================== */
/* PoP: da9_command_sync_state_path @ plugins/platforms/discord/adapter.py:DiscordAdapter._command_sync_state_path */
static json_t *da9_command_sync_state_path(da9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *home = da9_call(ctx, "get_hermes_home", json_object());
    if (da9_is_raise(home)) return home;
    char *h = da9_pystr_full(home);
    char *d = da9_path_join(h, "gateway");
    json_t *a = json_object();
    json_set(a, "parents", json_bool(true));
    json_set(a, "exist_ok", json_bool(true));
    json_t *mk = da9_call(ctx, "mkdir", a);
    if (da9_is_raise(mk)) {
        const char *k = da9_raise_kind(mk);
        if (da9_is_base_exc(k)) { free(h); free(d); return mk; } /* except Exception: pass */
        json_free(mk);
    } else json_free(mk);
    char *p = da9_path_join(d, "discord_command_sync_state.json");
    free(h); free(d);
    json_t *r = json_string(p);
    free(p);
    return r;
}

/* ==================== DEF 3: _cancel_bot_task ==================== */
/* PoP: da9_cancel_bot_task @ plugins/platforms/discord/adapter.py:DiscordAdapter._cancel_bot_task */
static json_t *da9_cancel_bot_task(da9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *task = da9_get(ctx->state, "_bot_task");
    if (da9_truthy(task)) {
        json_t *d = da9_call(ctx, "bot_task.done", json_object());
        if (da9_is_raise(d)) return d;
        bool done = da9_truthy(d); json_free(d);
        if (!done) {
            json_t *c = da9_call(ctx, "bot_task.cancel", json_object());
            if (da9_is_raise(c)) return c;
            json_free(c);
            json_t *aw = da9_call(ctx, "bot_task.await", json_object());
            if (da9_is_raise(aw)) {
                const char *k = da9_raise_kind(aw);
                if (!(da9_exc_extends(k, "Exception") || da9_exc_extends(k, "CancelledError")))
                    return aw;
                json_free(aw);
            } else json_free(aw);
        }
    }
    if (da9_has(ctx->state, "_bot_task")) json_set(ctx->state, "_bot_task", json_null());
    return json_null();
}

/* ==================== DEF 4: _cancel_liveness_task ==================== */
/* PoP: da9_cancel_liveness_task @ plugins/platforms/discord/adapter.py:DiscordAdapter._cancel_liveness_task */
static json_t *da9_cancel_liveness_task(da9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cur = da9_call(ctx, "asyncio.current_task", json_object());
    if (da9_is_raise(cur)) return cur;
    char *curs = da9_pystr_full(cur);
    const char *names[2] = {"_liveness_task", "_liveness_notification_task"};
    for (int i = 0; i < 2; i++) {
        json_t *task = da9_get(ctx->state, names[i]);
        if (da9_is_null(task)) continue;
        const char *ident = (task->type == JSON_OBJECT) ? json_get_str(task, "id", "") : NULL;
        if (ident && strcmp(ident, curs) == 0) continue; /* task is current */
        json_t *d = da9_call(ctx, "liveness.done", json_object());
        if (da9_is_raise(d)) { free(curs); json_free(cur); return d; }
        bool done = da9_truthy(d); json_free(d);
        if (!done) {
            json_t *c = da9_call(ctx, "liveness.cancel", json_object());
            if (da9_is_raise(c)) { free(curs); json_free(cur); return c; }
            json_free(c);
        }
        json_t *aw = da9_call(ctx, "liveness.await", json_object());
        if (da9_is_raise(aw)) {
            const char *k = da9_raise_kind(aw);
            if (da9_exc_extends(k, "CancelledError")) {
                json_free(aw); /* pass */
            } else if (da9_exc_extends(k, "Exception")) {
                json_free(aw);
                json_t *la = json_object();
                json_set_hdr(la, "fmt", "[%s] Liveness task shutdown failed");
                json_set_hdr(la, "name", json_get_str(ctx->state, "name", ""));
                json_set(la, "exc_info", json_bool(true));
                json_t *lr = da9_call(ctx, "logger.debug", la);
                if (da9_is_raise(lr)) { free(curs); json_free(cur); return lr; }
                json_free(lr);
            } else { free(curs); json_free(cur); return aw; }
        } else json_free(aw);
        json_set(ctx->state, names[i], json_null());
    }
    free(curs); json_free(cur);
    return json_null();
}

/* ==================== DEF 5: _advance_discord_recovery_cursor ==================== */
/* PoP: da9_advance_discord_recovery_cursor @ plugins/platforms/discord/adapter.py:DiscordAdapter._advance_discord_recovery_cursor */
static json_t *da9_advance_discord_recovery_cursor(da9_ctx_t *ctx, const json_t *args) {
    const char *ch = json_get_str(args, "channel_id", "");
    const char *mid = json_get_str(args, "message_id", "");
    if (!ch || !*ch || !mid || !*mid) return json_null();
    json_t *now = da9_call(ctx, "_utc_now_iso", json_object());
    if (da9_is_raise(now)) return now;
    json_t *a = json_object();
    json_set_hdr(a, "channel_id", ch);
    json_set_hdr(a, "message_id", mid);
    json_set(a, "now", now);
    json_t *r = da9_call(ctx, "conn.execute", a); /* _with_discord_recovery_db(op) */
    if (da9_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* ==================== DEF 6: _alert_adapters_and_config ==================== */
/* PoP: da9_alert_adapters_and_config @ plugins/platforms/discord/adapter.py:DiscordAdapter._alert_adapters_and_config */
static json_t *da9_alert_adapters_and_config(da9_ctx_t *ctx, const json_t *args) {
    json_t *profile = json_obj_get(args, "profile");
    json_t *out = json_array();
    if (!da9_truthy(profile)) {
        json_append(out, json_copy(da9_get(ctx->state, "adapters")));
        json_append(out, json_copy(da9_get(ctx->state, "config")));
        return out;
    }
    json_t *pa = json_object();
    json_set(pa, "profile", json_copy(profile));
    json_t *ad = da9_call(ctx, "_adapters_for_profile", pa);
    if (da9_is_raise(ad)) return ad;
    bool same = da9_str_eq(ad, "SAME");
    json_t *adapters = same ? json_copy(da9_get(ctx->state, "adapters")) : json_copy(ad);
    json_free(ad);
    if (same) {
        json_append(out, adapters);
        json_append(out, json_copy(da9_get(ctx->state, "config")));
        return out;
    }
    json_t *dir = da9_call(ctx, "get_profile_dir", json_object());
    if (da9_is_raise(dir)) { json_free(adapters); return dir; }
    json_free(dir);
    json_t *cfg = da9_call(ctx, "load_gateway_config", json_object());
    if (da9_is_raise(cfg)) { json_free(adapters); return cfg; }
    json_append(out, adapters);
    json_append(out, cfg);
    return out;
}

/* ==================== DEF 7: _apply_owner_only_visibility ==================== */
/* PoP: da9_apply_owner_only_visibility @ plugins/platforms/discord/adapter.py:DiscordAdapter._apply_owner_only_visibility */
static json_t *da9_apply_owner_only_visibility(da9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *pa = json_object(); json_set(pa, "value", json_number(0));
    json_t *perm = da9_call(ctx, "discord.Permissions", pa);
    if (da9_is_raise(perm)) {
        const char *k = da9_raise_kind(perm);
        if (!da9_exc_extends(k, "Exception")) return perm; /* BaseException */
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "[Discord] _apply_owner_only_visibility: cannot build Permissions(0): %s");
        json_set_hdr(la, "e", json_get_str(perm, "message", ""));
        json_t *lr = da9_call(ctx, "logger.warning", la);
        if (da9_is_raise(lr)) return lr;
        json_free(lr); json_free(perm);
        return json_null();
    }
    json_t *cmds = da9_call(ctx, "tree.get_commands", json_object());
    if (da9_is_raise(cmds)) { json_free(perm); return cmds; }
    long long applied = 0;
    if (cmds && cmds->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(cmds); i++) {
            json_t *c = json_get(cmds, i);
            json_t *sa = json_object();
            json_set(sa, "cmd", json_copy(c));
            json_set(sa, "value", json_copy(perm));
            json_t *r = da9_call(ctx, "set.default_permissions", sa);
            if (da9_is_raise(r)) {
                const char *k = da9_raise_kind(r);
                if (!da9_exc_extends(k, "Exception")) { json_free(cmds); json_free(perm); return r; }
                json_t *la = json_object();
                json_set_hdr(la, "fmt", "[Discord] Could not set default_permissions on %r: %s");
                json_set_hdr(la, "name", json_get_str(c, "name", "?"));
                json_set_hdr(la, "e", json_get_str(r, "message", ""));
                json_t *lr = da9_call(ctx, "logger.debug", la);
                if (da9_is_raise(lr)) { json_free(cmds); json_free(perm); return lr; }
                json_free(lr); json_free(r);
            } else { applied++; json_free(r); }
        }
    }
    json_free(cmds); json_free(perm);
    json_t *la = json_object();
    json_set_hdr(la, "fmt", "[Discord] Hid %d slash command(s) from non-admin guild members (opt-in defense in depth via DISCORD_HIDE_SLASH_COMMANDS).");
    json_set(la, "applied", json_number((double)applied));
    json_t *lr = da9_call(ctx, "logger.info", la);
    if (da9_is_raise(lr)) return lr;
    json_free(lr);
    return json_null();
}


/* ==================== DEF 8: _build_slash_event ==================== */
/* PoP: da9_build_slash_event @ plugins/platforms/discord/adapter.py:DiscordAdapter._build_slash_event */
static json_t *da9_build_slash_event(da9_ctx_t *ctx, const json_t *args) {
    json_t *inter = json_obj_get(args, "interaction");
    json_t *channel = da9_get(inter, "channel");
    json_t *user = da9_get(inter, "user");
    const char *kind = json_get_str(channel, "__kind__", "group");
    bool is_dm = (strcmp(kind, "dm") == 0);
    bool is_thread = (strcmp(kind, "thread") == 0);
    const char *chat_type = is_dm ? "dm" : (is_thread ? "thread" : "group");
    json_t *thread_id = json_null();
    if (is_thread) thread_id = json_string(da9_pystr_full(json_obj_get(inter, "channel_id")));
    /* chat_name */
    char *chat_name = da9_strdup("");
    if (is_dm) {
        char *n = da9_pystr_full(da9_get(user, "name"));
        free(chat_name); chat_name = n;
    } else if (is_thread) {
        json_t *a = json_object(); json_set(a, "channel", json_copy(channel));
        json_t *r = da9_call(ctx, "_format_thread_chat_name", a);
        if (da9_is_raise(r)) return r;
        free(chat_name); chat_name = da9_pystr_full(r); json_free(r);
    } else if (da9_has(channel, "name")) {
        free(chat_name);
        chat_name = da9_pystr_full(da9_get(channel, "name"));
        if (da9_has(channel, "guild") && da9_truthy(da9_get(channel, "guild"))) {
            char *gname = da9_pystr_full(da9_get(da9_get(channel, "guild"), "name"));
            char *joined = NULL;
            size_t n = strlen(gname) + strlen(chat_name) + 8;
            joined = malloc(n);
            if (joined) snprintf(joined, n, "%s / #%s", gname, chat_name);
            free(gname); free(chat_name); chat_name = joined ? joined : da9_strdup("");
        }
    }
    /* chat_topic */
    json_t *ta = json_object();
    json_set(ta, "channel", json_copy(channel));
    json_set(ta, "is_thread", json_bool(is_thread));
    json_t *chat_topic = da9_call(ctx, "_get_effective_topic", ta);
    if (da9_is_raise(chat_topic)) { free(chat_name); return chat_topic; }
    /* parent_id */
    json_t *parent_raw = json_null();
    char *parent_str = da9_strdup("");
    if (is_thread) {
        json_t *a = json_object(); json_set(a, "channel", json_copy(channel));
        json_t *r = da9_call(ctx, "_get_parent_channel_id", a);
        if (da9_is_raise(r)) { free(chat_name); free(parent_str); json_free(chat_topic); return r; }
        if (da9_truthy(r)) {
            parent_raw = json_copy(r);
            free(parent_str); parent_str = da9_pystr_full(r);
        }
        json_free(r);
    }
    /* guild_id, role_authorized */
    json_t *ia = json_object(); json_set(ia, "interaction", json_copy(inter));
    json_t *guild = da9_call(ctx, "_interaction_guild_id", json_copy(ia));
    if (da9_is_raise(guild)) { free(chat_name); free(parent_str); json_free(chat_topic); return guild; }
    json_t *role = da9_call(ctx, "_slash_role_grant", ia);
    if (da9_is_raise(role)) { free(chat_name); free(parent_str); json_free(chat_topic); json_free(guild); return role; }
    /* build_source */
    json_t *sa = json_object();
    json_set_hdr(sa, "chat_id", da9_pystr_full(json_obj_get(inter, "channel_id")));
    json_set_hdr(sa, "chat_name", chat_name);
    json_set_hdr(sa, "chat_type", chat_type);
    json_set_hdr(sa, "user_id", da9_pystr_full(da9_get(user, "id")));
    json_set(sa, "user_name", json_copy(da9_get(user, "display_name")));
    json_set(sa, "thread_id", thread_id);
    json_set(sa, "chat_topic", chat_topic);
    json_set(sa, "guild_id", guild);
    json_set(sa, "parent_chat_id", (parent_str[0] != '\0') ? json_copy(parent_raw) : json_null());
    json_set(sa, "role_authorized", role);
    json_t *source = da9_call(ctx, "build_source", sa);
    if (da9_is_raise(source)) { free(parent_str); return source; }
    /* msg_type */
    const char *txt = json_get_str(args, "text", "");
    bool has_slash = txt && txt[0] == '/';
    /* channel_prompt / auto_skill */
    json_t *ca1 = json_object();
    json_set_hdr(ca1, "channel_id", da9_pystr_full(json_obj_get(inter, "channel_id")));
    json_set(ca1, "parent_id", (parent_str[0] != '\0') ? json_copy(parent_raw) : json_null());
    json_t *prompt = da9_call(ctx, "_resolve_channel_prompt", ca1);
    if (da9_is_raise(prompt)) { free(parent_str); json_free(source); return prompt; }
    json_t *ca2 = json_copy(ca1);
    json_t *skills = da9_call(ctx, "_resolve_channel_skills", ca2);
    if (da9_is_raise(skills)) { free(parent_str); json_free(source); json_free(prompt); return skills; }
    /* MessageEvent */
    json_t *ma = json_object();
    json_set_hdr(ma, "text", txt);
    json_set_hdr(ma, "message_type", has_slash ? "COMMAND" : "TEXT");
    json_set(ma, "source", source);
    json_set(ma, "raw_message", json_copy(inter));
    json_set(ma, "channel_prompt", prompt);
    json_set(ma, "auto_skill", skills);
    free(chat_name); free(parent_str);
    return da9_call(ctx, "MessageEvent", ma);
}

/* ==================== DEF 9: _component_live_auth ==================== */
/* PoP: da9_component_live_auth @ plugins/platforms/discord/adapter.py:DiscordAdapter._component_live_auth */
static json_t *da9_component_live_auth(da9_ctx_t *ctx, const json_t *args) {
    json_t *inter = json_obj_get(args, "interaction");
    json_t *user = da9_get(inter, "user");
    char *uid = "";
    json_t *uidv = da9_is_null(user) ? NULL : da9_get(user, "id");
    if (uidv && !da9_is_null(uidv)) uid = da9_pystr_full(uidv);
    json_t *ca = json_object();
    json_set(ca, "interaction", json_copy(inter));
    json_set_hdr(ca, "thread", "Thread");
    json_t *res = da9_call(ctx, "component_chat_context", ca);
    if (da9_is_raise(res)) { if (uid && *uid) free(uid); return res; }
    if (res && res->type == JSON_ARRAY && json_len(res) != 3) {
        char msg[160];
        if (json_len(res) < 3)
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 3, got %zu)", json_len(res));
        else snprintf(msg, sizeof msg, "too many values to unpack (expected 3)");
        if (uid && *uid) free(uid);
        return da9_err("ValueError", msg);
    }
    json_t *chat_type = (res && res->type == JSON_ARRAY && json_len(res) > 0) ? json_copy(json_get(res, 0)) : json_null();
    json_t *chat_id = (res && res->type == JSON_ARRAY && json_len(res) > 1) ? json_copy(json_get(res, 1)) : json_null();
    json_t *thread_id = (res && res->type == JSON_ARRAY && json_len(res) > 2) ? json_copy(json_get(res, 2)) : json_null();
    json_free(res);
    json_t *a = json_object();
    json_set_hdr(a, "user_id", uid);
    json_set(a, "chat_type", chat_type);
    json_set(a, "chat_id", chat_id);
    json_set(a, "thread_id", thread_id);
    return da9_call(ctx, "_is_sender_authorized", a);
}

/* ==================== DEF 10: _auto_create_thread ==================== */
/* PoP: da9_auto_create_thread @ plugins/platforms/discord/adapter.py:DiscordAdapter._auto_create_thread */
static json_t *da9_auto_create_thread(da9_ctx_t *ctx, const json_t *args) {
    json_t *msg = json_obj_get(args, "message");
    json_t *na = json_object();
    json_t *cv = da9_get(msg, "content");
    json_set(na, "content", da9_truthy(cv) ? json_copy(cv) : json_string(""));
    json_t *nm = da9_call(ctx, "_derive_auto_thread_name", na);
    if (da9_is_raise(nm)) return nm;
    char *tname = da9_pystr_full(nm); json_free(nm);
    json_t *author = da9_get(msg, "author");
    const char *dn = json_get_str(author, "display_name", "");
    char *display = (dn && *dn) ? da9_strdup(dn) : da9_strdup("unknown user");
    char reason[1024];
    snprintf(reason, sizeof reason, "Auto-threaded from mention by %s", display);
    free(display);
    char *last_direct = da9_strdup("None");
    char *last_fallback = da9_strdup("None");
    for (int attempt = 0; attempt < 2; attempt++) {
        json_t *direct_err = NULL;
        json_t *da = json_object();
        json_set_hdr(da, "name", tname);
        json_set(da, "auto_archive_duration", json_number(1440));
        json_t *th = da9_call(ctx, "create_thread", da);
        if (!da9_is_raise(th)) {
            json_t *sa = json_object();
            json_set(sa, "thread", th);
            json_set_hdr(sa, "name", tname);
            json_t *st = da9_call(ctx, "stamp", sa);
            if (!da9_is_raise(st)) {
                free(tname); free(last_direct); free(last_fallback);
                return st;
            }
            const char *k = da9_raise_kind(st);
            if (da9_is_base_exc(k)) { free(tname); free(last_direct); free(last_fallback); return st; }
            direct_err = st;
        } else {
            const char *k = da9_raise_kind(th);
            if (da9_is_base_exc(k)) { free(tname); free(last_direct); free(last_fallback); return th; }
            direct_err = th;
        }
        free(last_direct); last_direct = da9_exc_str(direct_err); json_free(direct_err);
        /* fallback: try/catch(Exception) across t(), send(), create_thread(), stamp() */
        json_t *fallback_err = NULL;
        json_t *ta = json_object();
        json_set_hdr(ta, "key", "platform.discord.thread.seed_message");
        json_set_hdr(ta, "name", tname);
        json_t *seedtxt = da9_call(ctx, "t", ta);
        if (da9_is_raise(seedtxt)) {
            const char *k = da9_raise_kind(seedtxt);
            if (da9_is_base_exc(k)) { free(tname); free(last_direct); free(last_fallback); return seedtxt; }
            fallback_err = seedtxt;
        } else {
            json_t *sea = json_object();
            json_set(sea, "text", seedtxt);
            json_t *seedmsg = da9_call(ctx, "channel.send", sea);
            if (da9_is_raise(seedmsg)) {
                const char *k = da9_raise_kind(seedmsg);
                if (da9_is_base_exc(k)) { free(tname); free(last_direct); free(last_fallback); return seedmsg; }
                fallback_err = seedmsg;
            } else {
                json_t *sc = json_object();
                json_set_hdr(sc, "name", tname);
                json_set(sc, "auto_archive_duration", json_number(1440));
                json_set_hdr(sc, "reason", reason);
                json_t *sth = da9_call(ctx, "seed.create_thread", sc);
                if (!da9_is_raise(sth)) {
                    json_t *sa2 = json_object();
                    json_set(sa2, "thread", sth);
                    json_set_hdr(sa2, "name", tname);
                    json_t *st2 = da9_call(ctx, "stamp", sa2);
                    if (!da9_is_raise(st2)) {
                        json_free(seedmsg);
                        free(tname); free(last_direct); free(last_fallback);
                        return st2;
                    }
                    const char *k = da9_raise_kind(st2);
                    if (da9_is_base_exc(k)) { free(tname); free(last_direct); free(last_fallback); return st2; }
                    fallback_err = st2;
                } else {
                    const char *k = da9_raise_kind(sth);
                    if (da9_is_base_exc(k)) { free(tname); free(last_direct); free(last_fallback); return sth; }
                    fallback_err = sth;
                }
            }
            if (!fallback_err) json_free(seedmsg);
        }
        free(last_fallback);
        last_fallback = fallback_err ? da9_exc_str(fallback_err) : da9_strdup("None");
        if (fallback_err) json_free(fallback_err);
        if (attempt == 0) {
            json_t *sla = json_object();
            json_set(sla, "seconds", json_number(0.75));
            json_t *sl = da9_call(ctx, "sleep", sla);
            if (da9_is_raise(sl)) { free(tname); free(last_direct); free(last_fallback); return sl; }
            json_free(sl);
            continue;
        }
    }
    json_t *la = json_object();
    json_set_hdr(la, "fmt", "[%s] Auto-thread creation failed after retry. Direct error: %s. Fallback error: %s");
    json_set_hdr(la, "name", json_get_str(ctx->state, "name", ""));
    json_set_hdr(la, "direct", last_direct);
    json_set_hdr(la, "fallback", last_fallback);
    json_t *lr = da9_call(ctx, "logger.warning", la);
    if (da9_is_raise(lr)) { free(tname); free(last_direct); free(last_fallback); return lr; }
    json_free(lr);
    free(tname); free(last_direct); free(last_fallback);
    return json_null();
}


/* ==================== DEF 11/12: cache image / audio ==================== */
static json_t *da9_cache_image_audio(da9_ctx_t *ctx, const json_t *args, const char *mt, bool image) {
    json_t *att = json_obj_get(args, "att");
    const char *ext = json_get_str(args, "ext", "");
    json_t *ra = json_object();
    json_set(ra, "att", json_copy(att));
    json_set_hdr(ra, "media_type", mt);
    json_t *raw = da9_call(ctx, "_read_attachment_bytes", ra);
    if (da9_is_raise(raw)) return raw;
    if (!da9_is_null(raw)) {
        json_t *ba = json_object();
        json_set(ba, "data", json_copy(raw));
        json_set_hdr(ba, "ext", ext);
        json_t *cb = da9_call(ctx, "cache_bytes", ba);
        if (!da9_is_raise(cb)) return cb;
        const char *k = da9_raise_kind(cb);
        if (!da9_exc_extends(k, "Exception")) return cb;
        json_t *la = json_object();
        json_set_hdr(la, "fmt", image
            ? "[Discord] cache_image_from_bytes rejected att.read() data; falling back to URL: %s"
            : "[Discord] cache_audio_from_bytes failed; falling back to URL: %s");
        json_set_hdr(la, "e", json_get_str(cb, "message", ""));
        json_t *lr = da9_call(ctx, "logger.debug", la);
        if (da9_is_raise(lr)) return lr;
        json_free(lr); json_free(cb);
    }
    json_free(raw);
    json_t *ua = json_object();
    json_set_hdr(ua, "url", json_get_str(att, "url", ""));
    json_set_hdr(ua, "ext", ext);
    return da9_call(ctx, "cache_url", ua);
}
/* PoP: da9_cache_discord_image @ plugins/platforms/discord/adapter.py:DiscordAdapter._cache_discord_image */
static json_t *da9_cache_discord_image(da9_ctx_t *ctx, const json_t *args) {
    return da9_cache_image_audio(ctx, args, "image", true);
}
/* PoP: da9_cache_discord_audio @ plugins/platforms/discord/adapter.py:DiscordAdapter._cache_discord_audio */
static json_t *da9_cache_discord_audio(da9_ctx_t *ctx, const json_t *args) {
    return da9_cache_image_audio(ctx, args, "audio", false);
}

/* ==================== DEF 13: _cache_discord_document ==================== */
/* PoP: da9_cache_discord_document @ plugins/platforms/discord/adapter.py:DiscordAdapter._cache_discord_document */
static json_t *da9_cache_discord_document(da9_ctx_t *ctx, const json_t *args) {
    json_t *att = json_obj_get(args, "att");
    const char *url = json_get_str(att, "url", "");
    json_t *ra = json_object();
    json_set(ra, "att", json_copy(att));
    json_set_hdr(ra, "media_type", "document");
    json_t *raw = da9_call(ctx, "_read_attachment_bytes", ra);
    if (da9_is_raise(raw)) return raw;
    if (!da9_is_null(raw)) return raw;
    json_t *sa = json_object(); json_set_hdr(sa, "url", url);
    json_t *safe = da9_call(ctx, "is_safe_url", sa);
    if (da9_is_raise(safe)) return safe;
    bool is_safe = da9_truthy(safe); json_free(safe);
    if (!is_safe) {
        char msg[512];
        snprintf(msg, sizeof msg, "Blocked unsafe attachment URL (SSRF protection): %s", url);
        return da9_err("ValueError", msg);
    }
    json_t *pa = json_object(); json_set_hdr(pa, "platform_env_var", "DISCORD_PROXY");
    json_t *proxy = da9_call(ctx, "resolve_proxy_url", pa);
    if (da9_is_raise(proxy)) return proxy;
    json_t *ka = json_object(); json_set(ka, "proxy", proxy);
    json_t *kw = da9_call(ctx, "proxy_kwargs", ka);
    if (da9_is_raise(kw)) return kw;
    json_free(kw);
    json_t *ga = json_object();
    json_set_hdr(ga, "url", url);
    json_set(ga, "timeout", json_number(30));
    json_t *res = da9_call(ctx, "session.get", ga);
    if (da9_is_raise(res)) return res;
    long long status = 200;
    if (res && res->type == JSON_OBJECT) { long long s; if (da9_int(json_obj_get(res, "status"), &s)) status = s; }
    json_free(res);
    if (status != 200) {
        char msg[128]; snprintf(msg, sizeof msg, "HTTP %lld", status);
        return da9_err("Exception", msg);
    }
    return da9_call(ctx, "resp.read", json_object());
}

/* ==================== DEF 14: _cache_simple_media ==================== */
/* PoP: da9_cache_simple_media @ plugins/platforms/discord/adapter.py:DiscordAdapter._cache_simple_media */
static json_t *da9_cache_simple_media(da9_ctx_t *ctx, const json_t *args) {
    json_t *att = json_obj_get(args, "att");
    const char *ct = json_get_str(args, "content_type", "");
    const char *kind = json_get_str(args, "kind", "");
    json_t *exts = json_obj_get(args, "exts");
    const char *defext = json_get_str(args, "default_ext", "");
    /* ext = "." + content_type.split("/")[-1].split(";")[0] */
    char extbuf[512]; extbuf[0] = '.';
    const char *slash = strrchr(ct, '/');
    const char *semi = strchr(slash ? slash : ct, ';');
    size_t elen = semi ? (size_t)(semi - (slash ? slash + 1 : ct)) : strlen(slash ? slash + 1 : ct);
    if (elen > sizeof(extbuf) - 2) elen = sizeof(extbuf) - 2;
    if (elen) memcpy(extbuf + 1, (slash ? slash + 1 : ct), elen);
    extbuf[1 + elen] = '\0';
    bool in_exts = da9_in_set(exts, json_string(extbuf));
    const char *ext = in_exts ? extbuf : defext;
    json_t *ca = json_object();
    json_set(ca, "att", json_copy(att));
    json_set_hdr(ca, "content_type", ct);
    json_set_hdr(ca, "kind", kind);
    json_set(ca, "exts", json_copy(exts));
    json_set_hdr(ca, "default_ext", defext);
    json_set_hdr(ca, "ext", ext);
    json_t *cached = da9_call(ctx, "cache_media", ca);
    if (da9_is_raise(cached)) {
        const char *k = da9_raise_kind(cached);
        if (!da9_exc_extends(k, "Exception")) return cached;
        char msg[1024];
        snprintf(msg, sizeof msg, "[Discord] Failed to cache %s attachment: %s", kind, json_get_str(cached, "message", ""));
        json_t *pa = json_object(); json_set_hdr(pa, "text", msg);
        json_t *r = da9_call(ctx, "print", pa);
        if (da9_is_raise(r)) return r;
        json_free(r); json_free(cached);
        return json_string(json_get_str(att, "url", ""));
    }
    char msg[1024];
    snprintf(msg, sizeof msg, "[Discord] Cached user %s: %s", kind, da9_pystr_full(cached));
    json_t *pa = json_object(); json_set_hdr(pa, "text", msg);
    json_t *r = da9_call(ctx, "print", pa);
    if (da9_is_raise(r)) { json_free(cached); return r; }
    json_free(r);
    return cached;
}

/* ==================== DEF 15: _collect_attachment_media ==================== */
static const char *DA9_DOC_EXTS[] = {".txt", ".pdf", ".csv", ".md", ".json", ".log"};
static const char *DA9_DOC_MIMES[] = {"text/plain", "application/pdf", "text/csv", "text/markdown", "application/json", "text/plain"};
static const char *DA9_TEXT_EXTS[] = {".txt", ".csv", ".md", ".log"};
static int da9_doc_index(const char *ext) {
    for (int i = 0; i < 6; i++) if (ext && strcmp(ext, DA9_DOC_EXTS[i]) == 0) return i;
    return -1;
}
static bool da9_is_text_ext(const char *ext) {
    for (int i = 0; i < 4; i++) if (ext && strcmp(ext, DA9_TEXT_EXTS[i]) == 0) return true;
    return false;
}
/* PoP: da9_collect_attachment_media @ plugins/platforms/discord/adapter.py:DiscordAdapter._collect_attachment_media */
static json_t *da9_collect_attachment_media(da9_ctx_t *ctx, const json_t *args) {
    json_t *atts = json_obj_get(args, "all_attachments");
    json_t *urls = json_array(), *types = json_array(), *inlined = json_array();
    char *pending = NULL;
    size_t n = (atts && atts->type == JSON_ARRAY) ? json_len(atts) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *att = json_get(atts, i);
        const char *ct0 = json_get_str(att, "content_type", "");
        const char *ctype = (ct0 && *ct0) ? ct0 : "unknown";
        bool image = da9_startswith(ctype, "image/");
        bool audio = da9_startswith(ctype, "audio/");
        if (image || audio) {
            json_t *ca = json_object();
            json_set(ca, "att", json_copy(att));
            json_set_hdr(ca, "content_type", ctype);
            json_set_hdr(ca, "kind", image ? "image" : "audio");
            json_set_hdr(ca, "default_ext", image ? ".jpg" : ".ogg");
            json_t *url = da9_call(ctx, "cache_simple", ca);
            if (da9_is_raise(url)) return url;
            json_append(urls, url);
            json_append(types, json_string(ctype));
            json_append(inlined, json_bool(false));
            continue;
        }
        /* document */
        char *ext = da9_splitext(json_get_str(att, "filename", ""));
        char *extlow = da9_lower(ext); free(ext); ext = extlow;
        if (!*ext && *ctype) {
            for (int k = 0; k < 6; k++) if (strcmp(DA9_DOC_MIMES[k], ctype) == 0) {
                free(ext); ext = da9_strdup(DA9_DOC_EXTS[k]); break;
            }
        }
        int di = da9_doc_index(ext);
        bool allow = di >= 0;
        json_t *mx = da9_call(ctx, "max_doc_bytes", json_object());
        if (da9_is_raise(mx)) { free(ext); return mx; }
        long long cap = 0; bool havecap = da9_int(mx, &cap); json_free(mx);
        json_t *szv = json_obj_get(att, "size");
        long long sz = 0; bool havesz = da9_int(szv, &sz);
        if (havecap && cap && havesz && sz && sz > cap) {
            json_t *la = json_object();
            json_set_hdr(la, "fmt", "[Discord] Document too large (%s bytes > cap %s), skipping: %s");
            json_set(la, "size", szv ? json_copy(szv) : json_null());
            json_set(la, "cap", json_number((double)cap));
            json_set_hdr(la, "filename", json_get_str(att, "filename", ""));
            json_t *lr = da9_call(ctx, "logger.warning", la);
            if (da9_is_raise(lr)) { free(ext); return lr; }
            json_free(lr); free(ext);
            continue;
        }
        /* try block */
        json_t *da = json_object();
        json_set(da, "att", json_copy(att));
        json_set_hdr(da, "ext", ext);
        json_t *raw = da9_call(ctx, "cache_document", da);
        json_t *err = NULL;
        if (da9_is_raise(raw)) err = raw;
        if (!err) {
            char fname[1024];
            const char *fn = json_get_str(att, "filename", "");
            if (fn && *fn) snprintf(fname, sizeof fname, "%s", fn);
            else snprintf(fname, sizeof fname, "document%s", *ext ? ext : ".bin");
            json_t *ba = json_object();
            json_set(ba, "data", json_copy(raw));
            json_set_hdr(ba, "filename", fname);
            json_t *cached = da9_call(ctx, "cache_doc_bytes", ba);
            if (da9_is_raise(cached)) { err = cached; json_free(raw); }
            else {
                const char *docmime = allow ? DA9_DOC_MIMES[di]
                    : ((ctype && strcmp(ctype, "unknown") != 0) ? ctype : "application/octet-stream");
                json_append(urls, cached);
                json_append(types, json_string(docmime));
                json_append(inlined, json_bool(false));
                json_t *la = json_object();
                json_set_hdr(la, "fmt", "[Discord] Cached user %s: %s");
                json_set_hdr(la, "kind", allow ? "document" : "attachment");
                json_set(la, "path", json_copy(cached));
                json_t *lr = da9_call(ctx, "logger.info", la);
                if (da9_is_raise(lr)) { err = lr; }
                else {
                    json_free(lr);
                    bool is_text = da9_is_text_ext(ext) || da9_startswith(ctype, "text/");
                    size_t rawlen = (raw && raw->type == JSON_STRING && raw->str_val) ? strlen(raw->str_val) : 0;
                    if (is_text && rawlen <= 100 * 1024) {
                        char dname[1024];
                        const char *fn2 = json_get_str(att, "filename", "");
                        if (fn2 && *fn2) snprintf(dname, sizeof dname, "%s", fn2);
                        else snprintf(dname, sizeof dname, "document%s", *ext ? ext : ".txt");
                        json_t *ra = json_object();
                        json_set_hdr(ra, "pattern", "[^\\w.\\- ]");
                        json_set_hdr(ra, "repl", "_");
                        json_set_hdr(ra, "string", dname);
                        json_t *sub = da9_call(ctx, "re.sub", ra);
                        if (da9_is_raise(sub)) err = sub;
                        else {
                            const char *dn = (sub && sub->type == JSON_STRING)
                                ? sub->str_val : json_get_str(sub, "value", "");
                            if (!dn) dn = "";
                            char inj[65536];
                            snprintf(inj, sizeof inj, "[Content of %s]:\n%s", dn,
                                     (raw && raw->type == JSON_STRING && raw->str_val) ? raw->str_val : "");
                            if (pending && *pending) {
                                size_t need = strlen(pending) + strlen(inj) + 3;
                                char *nb = malloc(need);
                                if (nb) { snprintf(nb, need, "%s\n\n%s", pending, inj); free(pending); pending = nb; }
                            } else { free(pending); pending = da9_strdup(inj); }
                            json_array_set(inlined, json_len(inlined) - 1, json_bool(true));
                            json_free(sub);
                        }
                    }
                }
                json_free(raw);
            }
        }
        if (err) {
            const char *k = da9_raise_kind(err);
            bool ok = da9_exc_extends(k, "Exception");
            if (!ok) { free(ext); return err; }
            json_t *la = json_object();
            json_set_hdr(la, "fmt", "[Discord] Failed to cache document %s: %s");
            json_set_hdr(la, "filename", json_get_str(att, "filename", ""));
            json_set_hdr(la, "e", json_get_str(err, "message", ""));
            json_t *lr = da9_call(ctx, "logger.warning", la);
            if (da9_is_raise(lr)) { free(ext); return lr; }
            json_free(lr); json_free(err);
        }
        free(ext);
    }
    json_t *out = json_array();
    json_append(out, urls); json_append(out, types); json_append(out, inlined);
    json_append(out, (pending && *pending) ? json_string(pending) : json_null());
    free(pending);
    return out;
}


/* ==================== DEF 16: _apply_yaml_config ==================== */
static json_t *da9_env_set(da9_ctx_t *ctx, const char *key, const char *val) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set_hdr(a, "value", val);
    return da9_call(ctx, "env.set", a);
}
#define DA9_ENV(K, V) do { char *_v = (V); json_t *_r = da9_env_set(ctx, (K), _v ? _v : ""); \
                           free(_v); if (da9_is_raise(_r)) return _r; json_free(_r); } while (0)
static json_t *da9_yaml_gate(da9_ctx_t *ctx, json_t *seeded, json_t *dc, json_t *pextra,
                            const char *key, const char *env_key, bool from_extra, bool lower) {
    json_t *v = da9_has(dc, key) ? json_obj_get(dc, key) : (from_extra ? da9_get(pextra, key) : NULL);
    if (da9_is_null(v)) return NULL;
    char *text;
    if (lower) text = da9_lower_pystr(v);
    else { json_t *c = da9_csv(v); text = da9_strdup(c->str_val ? c->str_val : ""); json_free(c); }
    json_set(seeded, key, json_string(text));
    json_t *r = da9_env_set(ctx, env_key, text);
    free(text);
    return da9_is_raise(r) ? r : NULL;
}
/* PoP: da9_apply_yaml_config @ plugins/platforms/discord/adapter.py:_apply_yaml_config */
static json_t *da9_apply_yaml_config(da9_ctx_t *ctx, const json_t *args) {
    json_t *yaml_cfg = json_obj_get(args, "yaml_cfg");
    json_t *dc = json_obj_get(args, "discord_cfg");
    json_t *seeded = json_object();
    json_t *r;
    /* _YAML_BOOL_ENV_KEYS */
    static const char *BK[][2] = {
        {"require_mention", "DISCORD_REQUIRE_MENTION"},
        {"thread_require_mention", "DISCORD_THREAD_REQUIRE_MENTION"},
        {"bots_require_inline_mention", "DISCORD_BOTS_REQUIRE_INLINE_MENTION"},
    };
    for (int i = 0; i < 3; i++) if (da9_has(dc, BK[i][0])) {
        json_t *v = json_obj_get(dc, BK[i][0]);
        json_set(seeded, BK[i][0], json_copy(v));
        DA9_ENV(BK[i][1], da9_lower_pystr(v));
    }
    /* platform extra */
    json_t *pextra = json_object();
    json_t *platforms = da9_get(yaml_cfg, "platforms");
    if (platforms && platforms->type == JSON_OBJECT) {
        json_t *dpc = da9_get(platforms, "discord");
        if (dpc && dpc->type == JSON_OBJECT) {
            json_t *e = da9_get(dpc, "extra");
            if (e && e->type == JSON_OBJECT) pextra = e;
        }
    }
    r = da9_yaml_gate(ctx, seeded, dc, pextra, "allow_from", "DISCORD_ALLOWED_USERS", true, false); if (r) return r;
    r = da9_yaml_gate(ctx, seeded, dc, pextra, "allowed_roles", "DISCORD_ALLOWED_ROLES", true, false); if (r) return r;
    r = da9_yaml_gate(ctx, seeded, dc, pextra, "allow_all_users", "DISCORD_ALLOW_ALL_USERS", true, true); if (r) return r;
    r = da9_yaml_gate(ctx, seeded, dc, pextra, "allow_bots", "DISCORD_ALLOW_BOTS", true, true); if (r) return r;
    /* approval_mentions */
    json_t *am = da9_has(dc, "approval_mentions") ? json_obj_get(dc, "approval_mentions") : da9_get(pextra, "approval_mentions");
    if (!da9_is_null(am)) {
        json_set(seeded, "approval_mentions", json_copy(am));
        DA9_ENV("DISCORD_APPROVAL_MENTIONS", da9_lower_pystr(am));
    }
    r = da9_yaml_gate(ctx, seeded, dc, pextra, "free_response_channels", "DISCORD_FREE_RESPONSE_CHANNELS", false, false); if (r) return r;
    static const char *BK2[][2] = {
        {"auto_thread", "DISCORD_AUTO_THREAD"},
        {"free_response_auto_thread", "DISCORD_FREE_RESPONSE_AUTO_THREAD"},
        {"reactions", "DISCORD_REACTIONS"},
    };
    for (int i = 0; i < 3; i++) if (da9_has(dc, BK2[i][0])) {
        json_t *v = json_obj_get(dc, BK2[i][0]);
        json_set(seeded, BK2[i][0], json_copy(v));
        DA9_ENV(BK2[i][1], da9_lower_pystr(v));
    }
    json_t *bf = da9_get(dc, "missed_message_backfill");
    if (bf && bf->type == JSON_OBJECT) json_set(seeded, "missed_message_backfill", json_copy(bf));
    r = da9_yaml_gate(ctx, seeded, dc, pextra, "ignored_channels", "DISCORD_IGNORED_CHANNELS", false, false); if (r) return r;
    r = da9_yaml_gate(ctx, seeded, dc, pextra, "allowed_channels", "DISCORD_ALLOWED_CHANNELS", false, false); if (r) return r;
    r = da9_yaml_gate(ctx, seeded, dc, pextra, "no_thread_channels", "DISCORD_NO_THREAD_CHANNELS", false, false); if (r) return r;
    if (da9_has(dc, "history_backfill")) {
        json_t *v = json_obj_get(dc, "history_backfill");
        json_set(seeded, "history_backfill", json_copy(v));
        DA9_ENV("DISCORD_HISTORY_BACKFILL", da9_lower_pystr(v));
    }
    json_t *hbl = da9_get(dc, "history_backfill_limit");
    if (!da9_is_null(hbl)) {
        json_set(seeded, "history_backfill_limit", json_copy(hbl));
        DA9_ENV("DISCORD_HISTORY_BACKFILL_LIMIT", da9_pystr_full(hbl));
    }
    json_t *amc = da9_get(dc, "allow_mentions");
    if (amc && amc->type == JSON_OBJECT) {
        json_set(seeded, "allow_mentions", json_copy(amc));
        static const char *AK[] = {"everyone", "roles", "users", "replied_user"};
        for (int i = 0; i < 4; i++) if (da9_has(amc, AK[i])) {
            char up[64]; snprintf(up, sizeof up, "%s", AK[i]);
            for (char *q = up; *q; q++) *q = (char)toupper((unsigned char)*q);
            char key[96]; snprintf(key, sizeof key, "DISCORD_ALLOW_MENTION_%s", up);
            json_t *v = json_obj_get(amc, AK[i]);
            DA9_ENV(key, da9_lower_pystr(v));
        }
    }
    /* reply_to_mode */
    json_t *dextra = da9_get(dc, "extra");
    if (!dextra || dextra->type != JSON_OBJECT) dextra = NULL;
    json_t *rtm = da9_has(dc, "reply_to_mode") ? json_obj_get(dc, "reply_to_mode")
                                                : (dextra ? da9_get(dextra, "reply_to_mode") : NULL);
    if (!da9_is_null(rtm)) {
        char *val = (rtm && rtm->type == JSON_BOOL && !rtm->bool_val)
                    ? da9_strdup("off") : da9_lower_pystr(rtm);
        DA9_ENV("DISCORD_REPLY_TO_MODE", val);
    }
    /* merged websocket liveness config: {**dextra, **dc} */
    json_t *merged = json_object();
    if (dextra) for (size_t i = 0; i < dextra->c.count; i++)
        json_set(merged, dextra->c.keys[i], json_copy(dextra->c.items[i]));
    for (size_t i = 0; i < dc->c.count; i++)
        json_set(merged, dc->c.keys[i], json_copy(dc->c.items[i]));
    static const char *WK[][3] = {
        {"websocket_liveness_interval_seconds", "liveness_interval_seconds", "HERMES_DISCORD_LIVENESS_INTERVAL_SECONDS"},
        {"websocket_liveness_failure_threshold", "liveness_failure_threshold", "HERMES_DISCORD_LIVENESS_FAILURE_THRESHOLD"},
        {"websocket_heartbeat_ack_max_age_seconds", NULL, NULL},
        {"websocket_max_latency_seconds", NULL, NULL},
        {"websocket_event_max_silence_seconds", NULL, NULL},
    };
    for (int i = 0; i < 5; i++) {
        json_t *v = da9_get(merged, WK[i][0]);
        if (da9_is_null(v) && WK[i][1]) v = da9_get(merged, WK[i][1]);
        if (da9_is_null(v)) continue;
        json_set(seeded, WK[i][0], json_copy(v));
        if (WK[i][2]) DA9_ENV(WK[i][2], da9_pystr_full(v));
    }
    json_free(merged);
    if (seeded->c.count == 0) { json_free(seeded); return json_null(); }
    return seeded;
}

