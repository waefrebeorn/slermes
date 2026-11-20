/*
 * scale_to_zero_flow2.c — gateway/scale_to_zero.py FLOW-remainder cores
 * (universal-seam convention, sibling of scale_to_zero_flow.c / run_shutdown_flow2.c).
 *
 * Ports the DECISION layer of the scale-to-zero module — the 8 functions NOT
 * claimed by scale_to_zero_flow.c (which owns the 6 suspend-layer cores):
 * the Labs-toggle env predicate, the idle-timeout float coercion, the
 * relay-only platform set, the arm/idle predicates, the dashboard-client
 * heartbeat path/touch/last_seen trio. Call order, short-circuit branches,
 * conditional-expression laziness, try-region boundaries and exception->
 * fallback paths of the live Python are transcribed statement by statement.
 * Every live I/O (os.environ, the hermes_constants from-import + call,
 * os.path.dirname / os.makedirs / open / close / os.utime / os.stat,
 * time.time, logger.debug) enters through stz2_seams_t.call(ctx, name, args);
 * args is OWNED by stz2_call (freed after the borrowed call); results are NEW
 * json_t with NULL == Python None and {"__raise__":K,"msg":M} == a raised
 * exception. BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 * CancelledError/BaseExceptionGroup) pass through the `except Exception` site
 * in touch_dashboard_client_heartbeat (they are not Exception descendants).
 *
 * Self-contained: libjson only (no cross-object symbols). All helpers static stz2_*.
 *
 * Modeling notes (differential oracle tests/oracle_scale_to_zero_flow2.py):
 *  - environ is a JSON object (dict) or null (None -> the "os.environ" seam).
 *    A scripted-null os.environ makes `.get` raise AttributeError 'NoneType'.
 *    env values are str/int/bool/null; integral-FLOAT values are excluded from
 *    str()-computed positions (libjson has no int/float identity).
 *  - float() coercion (parse_idle_timeout_seconds): the string parser strips
 *    the C-isspace set (ASCII \t \n \v \f \r space — NOT \x1c-\x1f, which
 *    str.strip DOES drop), accepts +/- signs, inf/infinity/nan (case-folded),
 *    underscores ONLY between two digits, one optional dot and one optional
 *    exponent. The ValueError message carries repr() of the RAW (unstripped)
 *    token: \n \r \t short forms, other <0x20 and 0x7f -> \xNN (lowercase).
 *    Token domain: printable ASCII without quotes/backslashes (repr quoting
 *    unambiguous). inf/nan RESULTS are outside the JSON-serializable fixture
 *    domain (no non-finite fixtures). The `float(default_minutes)` branch is
 *    CONDITIONAL: evaluated only when minutes <= 0 (a null default_minutes
 *    with a positive cfg_value never raises).
 *  - platforms (messaging_is_relay_only_or_absent) iterates per Python: array
 *    -> elements, str -> ASCII characters, object -> keys in insertion order;
 *    null/number/bool raise TypeError "'<T>' object is not iterable". Object
 *    ELEMENTS model SimpleNamespace(**obj): getattr(p,"value",p) reads the
 *    "value" key when present, else str() of the namespace renders
 *    "namespace(k=<repr>, ...)" in insertion order (scalar values only:
 *    str/int/bool/null; str values printable ASCII without quotes/backslashes).
 *    str()/strip()/lower() use the White_Space set (incl \x1c-\x1f) / ASCII.
 *  - Comparisons (is_idle, min in last_seen): number/bool operands compare
 *    numerically (bool as 0/1); str/str compares lexicographically (ASCII ==
 *    codepoint order); mixed/other operands raise the py3.14 TypeError
 *    "'<op>' not supported between instances of '<T(a)>' and '<T(b)>'".
 *    min(a,b) compares b < a, so its TypeError names type(b) FIRST.
 *    list/dict operands are outside the fixture domain for the seconds/
 *    timeout/mtime/now positions (same-type container comparisons excluded).
 *  - Path model (dashboard_client_heartbeat_path): Path(home) / REL projects
 *    to the Windows pathlib as_posix() string. Backslashes fold to '/'; a
 *    char+':' drive prefix (ANY non-separator char, per ntpath.splitdrive) is
 *    preserved and its tail's leading slash run collapses to ONE root;
 *    without a drive, a leading run of exactly 2 is UNC (server + share
 *    components kept VERBATIM, even '.'/'..'/empty; a raw component after the
 *    share contributes a root '/' even when empty), a run of 3 keeps the
 *    FIRST component verbatim (the UNC share) and normalizes the rest, a run
 *    >= 4 collapses to '////' + fully normalized components. '.' and empty
 *    components drop elsewhere, '..' stays, internal runs collapse, trailing
 *    separators drop. Path("") == ".". Joining REL: empty/'.' -> REL alone;
 *    the bare UNC root "//" -> "//" + REL + "/" (probed py3.14 trailing-slash
 *    edge); a bare drive norm (exactly "X:", X any non-separator) concatenates
 *    REL with NO separator; a '/'-final norm concatenates directly; else one
 *    '/' is inserted. Non-str home raises the py3.14 Path() TypeError.
 *    Domain: ':' appears ONLY in the drive-prefix position — ':' inside
 *    components (UNC server/share, drive-like '1:1' '.'-guards) and device
 *    ('\\\\?\\') paths are probed exotic edges outside the lexical model.
 *    REL is os.path.join("state", "dashboard_clients.heartbeat") projected
 *    to POSIX components.
 *  - The from-import fires TWO seams in order: "import_get_process_hermes_home"
 *    {} (the module attribute fetch; only a raise matters) then
 *    "get_process_hermes_home" {} (the call). dashboard_client_heartbeat_path
 *    runs INLINE (real) inside touch/last_seen on both sides (ported sibling).
 *  - touch: open(p,"a",encoding="utf-8") is the "open" seam {path,mode,encoding};
 *    an OBJECT result models a file handle whose .close() fires the "close"
 *    seam {}; a NON-object result raises AttributeError "'<T>' object has no
 *    attribute 'close'" (attribute lookup fails before any close event).
 *    os.utime(p,None) is {path,times:null}. The except-Exception handler fires
 *    logger.debug {fmt,exc_info:true}; a raise FROM logger.debug propagates.
 *    path inputs are str|null (os.PathLike doubles are outside the JSON
 *    domain, so the ntpath.dirname fspath TypeError family is unreachable).
 *  - last_seen: os.stat returns an object -> SimpleNamespace(**obj) attribute
 *    carrier: a missing "st_mtime" raises AttributeError "'types.SimpleNamespace'
 *    object has no attribute 'st_mtime'" (MODULE-qualified); a NON-object stat
 *    result raises AttributeError "'<T>' object has no attribute 'st_mtime'".
 *    Neither is caught by the except-OSError sites (they propagate). The
 *    time.time / heartbeat-path resolutions sit OUTSIDE the try (raises
 *    propagate uncaught). FileNotFoundError -> None; other OSError -> current
 *    (fail-awake); min clamps to current.
 *  - Fail-closed: no seam channel installed -> RuntimeError "no seam channel
 *    installed for <name>"; the harness additionally fail-closes an ABSENT
 *    script key with RuntimeError "no script for <name>".
 *  - Ordered trace; integral floats == ints (canon folds). ASCII scope.
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
#include <ctype.h>
#include "libjson/json.h"

#define STZ2_REL "state/dashboard_clients.heartbeat"

typedef struct stz2_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} stz2_seams_t;

/* ── universal-seam helpers ── */
static json_t *stz2_call(const stz2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        char msg[160];
        snprintf(msg, sizeof(msg), "no seam channel installed for %s", name);
        json_t *r = json_object();
        json_set(r, "__raise__", json_string("RuntimeError"));
        json_set(r, "msg", json_string(msg));
        return r;
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static bool stz2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *stz2_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *stz2_emsg(const json_t *r) {
    const json_t *k = json_obj_get(r, "msg");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *stz2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *stz2_args(void) { return json_object(); }
static void stz2_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void stz2_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void stz2_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *stz2_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* ── full builtin exception hierarchy (py3.14) ──
 * Single-parent walk; ExceptionGroup is an Exception (also BaseExceptionGroup).
 * EnvironmentError/IOError/WindowsError are OSError aliases. CancelledError and
 * the BaseException trio/group sit OUTSIDE Exception so they pass through the
 * `except Exception` site in touch_dashboard_client_heartbeat. */
static const char *stz2_exc_parent(const char *k) {
    static const struct { const char *n, *p; } T[] = {
        {"Exception", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"KeyboardInterrupt", "BaseException"},
        {"SystemExit", "BaseException"},
        {"CancelledError", "BaseException"},
        {"ExceptionGroup", "Exception"},
        {"ArithmeticError", "Exception"},
        {"AssertionError", "Exception"},
        {"AttributeError", "Exception"},
        {"BufferError", "Exception"},
        {"EOFError", "Exception"},
        {"ImportError", "Exception"},
        {"LookupError", "Exception"},
        {"MemoryError", "Exception"},
        {"NameError", "Exception"},
        {"ReferenceError", "Exception"},
        {"RuntimeError", "Exception"},
        {"StopAsyncIteration", "Exception"},
        {"StopIteration", "Exception"},
        {"SyntaxError", "Exception"},
        {"SystemError", "Exception"},
        {"TypeError", "Exception"},
        {"ValueError", "Exception"},
        {"Warning", "Exception"},
        {"OSError", "Exception"},
        {"EnvironmentError", "OSError"},
        {"IOError", "OSError"},
        {"WindowsError", "OSError"},
        {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"},
        {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"},
        {"TimeoutError", "OSError"},
        {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"},
        {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"ModuleNotFoundError", "ImportError"},
        {"NotImplementedError", "RuntimeError"},
        {"PythonFinalizationError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"_IncompleteInputError", "SyntaxError"},
        {"UnboundLocalError", "NameError"},
        {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"BytesWarning", "Warning"},
        {"DeprecationWarning", "Warning"},
        {"EncodingWarning", "Warning"},
        {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"},
        {"ResourceWarning", "Warning"},
        {"RuntimeWarning", "Warning"},
        {"SyntaxWarning", "Warning"},
        {"UnicodeWarning", "Warning"},
        {"UserWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(T[i].n, k) == 0) return T[i].p;
    return NULL;
}
static bool stz2_is_a(const char *kind, const char *ancestor) {
    const char *k = kind;
    for (int depth = 0; k && depth < 64; depth++) {
        if (strcmp(k, ancestor) == 0) return true;
        k = stz2_exc_parent(k);
    }
    return false;
}
static bool stz2_is_exception(const char *k) { return stz2_is_a(k, "Exception"); }
static bool stz2_is_oserror(const char *k) { return stz2_is_a(k, "OSError"); }
static bool stz2_is_filenotfound(const char *k) { return stz2_is_a(k, "FileNotFoundError"); }
static bool stz2_is_typeerror(const char *k) { return stz2_is_a(k, "TypeError"); }
static bool stz2_is_valueerror(const char *k) { return stz2_is_a(k, "ValueError"); }

/* ── Python value helpers ── */
static bool stz2_truth(const json_t *v) {
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
static bool stz2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool stz2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool stz2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool stz2_is_num(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static const char *stz2_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *stz2_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d) return "int";
        return "float";
    }
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    return "dict";
}
static double stz2_numval(const json_t *v) {
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return v->num_val;
}
/* str() of a scalar (int/bool/str/None). Integral floats render as int (the
 * fixture domain excludes integral floats in str() positions). malloc'd. */
static char *stz2_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d) {
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
            return strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
/* White_Space codepoint set used by str.strip (incl \x1c-\x1f). */
static bool stz2_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F))
        return true;
    switch (cp) {
        case 0x85: case 0xA0: case 0x1680:
        case 0x2028: case 0x2029: case 0x202F: case 0x205F: case 0x3000:
            return true;
        default:
            return (cp >= 0x2000 && cp <= 0x200A);
    }
}
/* decode one UTF-8 codepoint at s; advances *i. ASCII fast path. */
static unsigned int stz2_next_cp(const char *s, size_t n, size_t *i) {
    unsigned char c = (unsigned char)s[*i];
    if (c < 0x80) { (*i)++; return c; }
    unsigned int cp = 0; int extra = 0;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { (*i)++; return 0xFFFD; }
    (*i)++;
    for (int k = 0; k < extra && *i < n; k++, (*i)++) {
        unsigned char cc = (unsigned char)s[*i];
        if ((cc & 0xC0) != 0x80) break;
        cp = (cp << 6) | (cc & 0x3F);
    }
    return cp;
}
/* str.strip(): drop leading/trailing White_Space codepoints. malloc'd. */
static char *stz2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t start = 0, end = n;
    size_t i = 0;
    while (i < n) {
        size_t save = i;
        unsigned int cp = stz2_next_cp(s, n, &i);
        if (!stz2_is_ws_cp(cp)) { start = save; break; }
        start = i;
    }
    i = start; size_t last = start;
    while (i < n) {
        unsigned int cp = stz2_next_cp(s, n, &i);
        if (!stz2_is_ws_cp(cp)) last = i;
    }
    end = last;
    if (end < start) end = start;
    size_t len = end - start;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s + start, len);
    out[len] = '\0';
    return out;
}
/* ASCII lower-case in place. */
static void stz2_lower(char *s) {
    if (!s) return;
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s += 32;
}
/* Python repr() of an ASCII str for error messages: surrounding single quotes,
 * \n \r \t short forms, other <0x20 and 0x7f -> \xNN (lowercase hex), ' and \
 * escaped. Fixture tokens exclude quotes/backslashes so quoting is unambiguous.
 * malloc'd. */
static char *stz2_repr_str(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return NULL;
    char *q = out;
    *q++ = '\'';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c == '\r') { *q++ = '\\'; *q++ = 'r'; }
        else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
        else if (c == '\'') { *q++ = '\\'; *q++ = '\''; }
        else if (c == '\\') { *q++ = '\\'; *q++ = '\\'; }
        else if (c < 0x20 || c == 0x7f) { q += snprintf(q, 5, "\\x%02x", c); }
        else *q++ = (char)c;
    }
    *q++ = '\'';
    *q = '\0';
    return out;
}
/* scalar repr for namespace() rendering: str -> quoted repr, else str(). */
static char *stz2_scalar_repr(const json_t *v) {
    if (stz2_is_str(v)) return stz2_repr_str(stz2_strval(v));
    return stz2_pystr(v);
}
/* str(SimpleNamespace(**obj)) for an object WITHOUT a "value" key:
 * "namespace(k1=<repr>, k2=<repr>)" in insertion order. malloc'd. */
static char *stz2_namespace_repr(const json_t *obj) {
    size_t cap = 32;
    for (size_t i = 0; i < obj->c.count; i++)
        cap += strlen(obj->c.keys[i]) + 24;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t k = 0;
    memcpy(out + k, "namespace(", 10); k += 10;
    for (size_t i = 0; i < obj->c.count; i++) {
        if (i) { out[k++] = ','; out[k++] = ' '; }
        size_t kl = strlen(obj->c.keys[i]);
        /* grow for long values */
        char *rv = stz2_scalar_repr(obj->c.items[i]);
        size_t need = k + kl + (rv ? strlen(rv) : 0) + 8;
        if (need > cap) {
            cap = need * 2;
            char *no = (char *)realloc(out, cap);
            if (!no) { free(rv); free(out); return NULL; }
            out = no;
        }
        memcpy(out + k, obj->c.keys[i], kl); k += kl;
        out[k++] = '=';
        if (rv) { size_t rl = strlen(rv); memcpy(out + k, rv, rl); k += rl; free(rv); }
    }
    out[k++] = ')';
    out[k] = '\0';
    return out;
}

/* ── float() coercion (py3.14) ──
 * Returns the double; on failure sets *kind and builds *msg (caller frees).
 * bool -> 0/1; number -> itself; str -> sign/inf/nan/underscore/dot/exponent
 * grammar over the C-isspace-stripped token; other types -> TypeError. */
static double stz2_py_float(const json_t *v, const char **kind, char **msg) {
    *kind = NULL; *msg = NULL;
    if (!v || v->type == JSON_NULL) {
        *kind = "TypeError";
        *msg = strdup("float() argument must be a string or a real number, not 'NoneType'");
        return 0.0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        const char *raw = v->str_val ? v->str_val : "";
        const char *s = raw;
        while (*s && isspace((unsigned char)*s)) s++;
        const char *end = s + strlen(s);
        while (end > s && isspace((unsigned char)end[-1])) end--;
        bool ok = true;
        bool neg = false;
        const char *p = s;
        if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
        size_t rem = (size_t)(end - p);
        /* inf / infinity / nan (case-insensitive, sign consumed first) */
        if (rem == 3 && (p[0]=='i'||p[0]=='I') && (p[1]=='n'||p[1]=='N') && (p[2]=='f'||p[2]=='F'))
            return neg ? -HUGE_VAL : HUGE_VAL;
        if (rem == 8 && (p[0]=='i'||p[0]=='I') && (p[1]=='n'||p[1]=='N') && (p[2]=='f'||p[2]=='F')
            && (p[3]=='i'||p[3]=='I') && (p[4]=='n'||p[4]=='N') && (p[5]=='i'||p[5]=='I')
            && (p[6]=='t'||p[6]=='T') && (p[7]=='y'||p[7]=='Y'))
            return neg ? -HUGE_VAL : HUGE_VAL;
        if (rem == 3 && (p[0]=='n'||p[0]=='N') && (p[1]=='a'||p[1]=='A') && (p[2]=='n'||p[2]=='N'))
            return NAN;
        /* underscores: every '_' strictly between two digits */
        for (const char *u = p; u < end; u++) {
            if (*u == '_') {
                if (u == p || u + 1 >= end) { ok = false; break; }
                if (!(u[-1] >= '0' && u[-1] <= '9')) { ok = false; break; }
                if (!(u[1] >= '0' && u[1] <= '9')) { ok = false; break; }
            }
        }
        /* grammar: digits [ '.' digits ] [ (e|E) [sign] digits ], >=1 mantissa
         * digit; underscores (validated above) are skipped in the scan. */
        if (ok) {
            const char *g = p;
            long nd = 0;
            while (g < end && ((*g >= '0' && *g <= '9') || *g == '_')) {
                if (*g != '_') nd++;
                g++;
            }
            if (g < end && *g == '.') {
                g++;
                while (g < end && ((*g >= '0' && *g <= '9') || *g == '_')) {
                    if (*g != '_') nd++;
                    g++;
                }
            }
            if (nd == 0) ok = false;
            if (ok && g < end && (*g == 'e' || *g == 'E')) {
                g++;
                if (g < end && (*g == '+' || *g == '-')) g++;
                long ne = 0;
                while (g < end && ((*g >= '0' && *g <= '9') || *g == '_')) {
                    if (*g != '_') ne++;
                    g++;
                }
                if (ne == 0) ok = false;
            }
            if (ok && g != end) ok = false;
        }
        if (!ok) {
            *kind = "ValueError";
            char *rp = stz2_repr_str(raw);
            size_t need = strlen("could not convert string to float: ") + (rp ? strlen(rp) : 0) + 1;
            char *m = (char *)malloc(need);
            if (m) snprintf(m, need, "could not convert string to float: %s", rp ? rp : "''");
            free(rp);
            *msg = m;
            return 0.0;
        }
        /* clean buffer: sign carried, underscores dropped */
        size_t clen = (size_t)(end - p);
        char *clean = (char *)malloc(clen + 2);
        if (!clean) { *kind = "MemoryError"; *msg = strdup(""); return 0.0; }
        size_t k = 0;
        if (neg) clean[k++] = '-';
        for (const char *u = p; u < end; u++) if (*u != '_') clean[k++] = *u;
        clean[k] = '\0';
        double d = strtod(clean, NULL);
        free(clean);
        return d;
    }
    *kind = "TypeError";
    {
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "float() argument must be a string or a real number, not '%s'",
                 stz2_typename(v));
        *msg = strdup(buf);
    }
    return 0.0;
}

/* ── comparisons (py3.14 messages) ── */
/* v <= 0 (the 0 literal is int). Returns json_bool or a TypeError raise. */
static json_t *stz2_le_zero(const json_t *v) {
    if (stz2_is_num(v)) return json_bool(stz2_numval(v) <= 0.0);
    char buf[160];
    snprintf(buf, sizeof(buf), "'<=' not supported between instances of '%s' and 'int'",
             stz2_typename(v));
    return stz2_raise("TypeError", buf);
}
/* a >= b. number/bool numeric; str/str lexicographic; else TypeError. */
static json_t *stz2_ge(const json_t *a, const json_t *b) {
    if (stz2_is_num(a) && stz2_is_num(b))
        return json_bool(stz2_numval(a) >= stz2_numval(b));
    if (stz2_is_str(a) && stz2_is_str(b))
        return json_bool(strcmp(stz2_strval(a), stz2_strval(b)) >= 0);
    char buf[192];
    snprintf(buf, sizeof(buf), "'>=' not supported between instances of '%s' and '%s'",
             stz2_typename(a), stz2_typename(b));
    return stz2_raise("TypeError", buf);
}
/* min(a, b): compares b < a; returns an OWNED copy of the chosen operand or a
 * TypeError naming type(b) FIRST. number/bool numeric; str/str lexicographic. */
static json_t *stz2_min2(const json_t *a, const json_t *b) {
    bool take_b = false;
    if (stz2_is_num(a) && stz2_is_num(b)) take_b = stz2_numval(b) < stz2_numval(a);
    else if (stz2_is_str(a) && stz2_is_str(b)) take_b = strcmp(stz2_strval(b), stz2_strval(a)) < 0;
    else {
        char buf[192];
        snprintf(buf, sizeof(buf), "'<' not supported between instances of '%s' and '%s'",
                 stz2_typename(b), stz2_typename(a));
        return stz2_raise("TypeError", buf);
    }
    return json_copy(take_b ? b : a);
}
/* AttributeError "'<T>' object has no attribute '<name>'". */
static json_t *stz2_attr_raise(const json_t *obj, const char *name) {
    char buf[192];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", stz2_typename(obj), name);
    return stz2_raise("AttributeError", buf);
}

/* ── Windows pathlib lexical model (probed py3.14) ──
 * path_norm folds backslashes; a char+':' drive prefix (any non-separator
 * char, per ntpath.splitdrive) is preserved and its tail's leading slash run
 * collapses to ONE root. Without a drive: a leading run of exactly 2 is UNC
 * (server + share components kept VERBATIM, even '.'/'..'/empty; a raw
 * component AFTER the share contributes a root '/' even when empty); a run
 * of 3 keeps the FIRST component verbatim (the UNC share) and normalizes the
 * rest; a run >= 4 collapses to '////' + fully normalized components. '.'
 * and empty components drop elsewhere, '..' stays, internal runs collapse,
 * trailing separators drop. Path("") == ".". malloc'd. */
static char *stz2_path_norm(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *tmp = (char *)malloc(n + 1);
    if (!tmp) return NULL;
    for (size_t i = 0; i < n; i++) tmp[i] = (s[i] == '\\') ? '/' : s[i];
    tmp[n] = '\0';
    size_t drive = 0;
    if (n >= 2 && tmp[1] == ':' && tmp[0] != '/') drive = 2;
    const char *rest = tmp + drive;
    size_t rn = strlen(rest);
    size_t lead = 0;
    while (lead < rn && rest[lead] == '/') lead++;
    /* tokenize rest+lead into RAW components (empties kept; the UNC share
     * slot and the post-share root depend on them) */
    const char *body = rest + lead;
    size_t bn = strlen(body);
    size_t ncomp = 0;
    if (bn > 0) {
        ncomp = 1;
        for (size_t i = 0; i < bn; i++) if (body[i] == '/') ncomp++;
    }
    char **comp = (char **)malloc((ncomp ? ncomp : 1) * sizeof(char *));
    size_t *clen = (size_t *)malloc((ncomp ? ncomp : 1) * sizeof(size_t));
    if (!comp || !clen) { free(comp); free(clen); free(tmp); return NULL; }
    size_t nc = 0;
    {
        size_t start = 0;
        for (size_t i = 0; i <= bn; i++) {
            if (i == bn || body[i] == '/') {
                comp[nc] = (char *)body + start;
                clen[nc] = i - start;
                nc++;
                start = i + 1;
            }
        }
    }
    /* normalized component join helper (drop '' and '.', keep the rest) */
    #define STZ2_NORM_COMP(FROM, OUT, OUTLEN) do { \
        size_t _k = 0; int _have = 0; \
        for (size_t _i = (FROM); _i < nc; _i++) { \
            if (clen[_i] == 0) continue; \
            if (clen[_i] == 1 && comp[_i][0] == '.') continue; \
            if (_have) (OUT)[_k++] = '/'; \
            memcpy((OUT) + _k, comp[_i], clen[_i]); _k += clen[_i]; \
            _have = 1; \
        } \
        (OUT)[_k] = '\0'; (OUTLEN) = _k; \
    } while (0)
    size_t cap = n + 16;
    char *out = (char *)malloc(cap);
    if (!out) { free(comp); free(clen); free(tmp); return NULL; }
    size_t k = 0;
    if (drive) {
        out[k++] = tmp[0]; out[k++] = ':';
        if (lead >= 1) out[k++] = '/';
        size_t jl = 0;
        STZ2_NORM_COMP(0, out + k, jl);
        k += jl;
    } else if (lead == 0) {
        if (nc == 0) { out[k++] = '.'; }
        else {
            size_t jl = 0;
            STZ2_NORM_COMP(0, out + k, jl);
            k += jl;
            if (jl == 0) out[k++] = '.';
        }
    } else if (lead == 1) {
        out[k++] = '/';
        size_t jl = 0;
        STZ2_NORM_COMP(0, out + k, jl);
        k += jl;
    } else if (lead == 2) {
        out[k++] = '/'; out[k++] = '/';
        if (nc >= 1) {
            memcpy(out + k, comp[0], clen[0]); k += clen[0];
            if (nc >= 2) {
                out[k++] = '/';
                memcpy(out + k, comp[1], clen[1]); k += clen[1];
                if (nc >= 3) out[k++] = '/';
                size_t jl = 0;
                char *jb = (char *)malloc(bn + 2);
                if (jb) {
                    STZ2_NORM_COMP(2, jb, jl);
                    if (jl > 0) { memcpy(out + k, jb, jl); k += jl; }
                    free(jb);
                }
            }
        }
    } else if (lead == 3) {
        out[k++] = '/'; out[k++] = '/'; out[k++] = '/';
        if (nc >= 1) {
            memcpy(out + k, comp[0], clen[0]); k += clen[0];
            if (nc >= 2) {
                out[k++] = '/';
                size_t jl = 0;
                char *jb = (char *)malloc(bn + 2);
                if (jb) {
                    STZ2_NORM_COMP(1, jb, jl);
                    if (jl > 0) { memcpy(out + k, jb, jl); k += jl; }
                    free(jb);
                }
            }
        }
    } else { /* lead >= 4: collapses to '////' + normalized components */
        out[k++] = '/'; out[k++] = '/'; out[k++] = '/'; out[k++] = '/';
        size_t jl = 0;
        STZ2_NORM_COMP(0, out + k, jl);
        k += jl;
    }
    out[k] = '\0';
    #undef STZ2_NORM_COMP
    free(comp); free(clen); free(tmp);
    return out;
}
/* Path(home) / REL projected to as_posix(). malloc'd. */
static char *stz2_path_join_rel(const char *norm) {
    if (!norm) norm = "";
    size_t nn = strlen(norm), rn = strlen(STZ2_REL);
    if (nn == 0 || strcmp(norm, ".") == 0) return strdup(STZ2_REL);
    if (strcmp(norm, "//") == 0) {
        char *out = (char *)malloc(nn + rn + 2);
        if (out) snprintf(out, nn + rn + 2, "%s%s/", norm, STZ2_REL);
        return out;
    }
    /* bare drive (X: — any non-separator char) concatenates with NO separator;
     * a '/'-final norm concatenates directly; else one '/' is inserted. */
    int no_sep = (norm[nn - 1] == '/')
        || (nn == 2 && norm[1] == ':' && norm[0] != '/');
    char *out = (char *)malloc(nn + rn + 2);
    if (out) snprintf(out, nn + rn + 2, "%s%s%s", norm, no_sep ? "" : "/", STZ2_REL);
    return out;
}

/* ── internal env-string leaf (NOT re-claimed: _env_str belongs to flow.c) ── */
static json_t *stz2_env_str(const json_t *env, const char *key, const stz2_seams_t *s) {
    json_t *E;
    if (stz2_is_null(env)) E = stz2_call(s, "os.environ", stz2_args());
    else E = json_copy(env);
    if (stz2_raised(E)) return E;
    if (!stz2_is_obj(E)) { json_t *r = stz2_attr_raise(E, "get"); json_free(E); return r; }
    const json_t *v = json_obj_get(E, key);
    char *raw = v ? stz2_pystr(v) : strdup("");
    char *stripped = stz2_strip(raw);
    free(raw);
    json_free(E);
    json_t *r = json_string(stripped ? stripped : "");
    free(stripped);
    return r;
}

/* ── exported flow cores (forward declarations) ── */
static json_t *stz2_scale_to_zero_enabled(const json_t *env, const stz2_seams_t *s);
static json_t *stz2_parse_idle_timeout_seconds(const json_t *cfg_value,
                                               const json_t *default_minutes,
                                               const stz2_seams_t *s);
static json_t *stz2_messaging_is_relay_only_or_absent(const json_t *platforms,
                                                      const stz2_seams_t *s);
static json_t *stz2_should_arm(const json_t *enabled, const json_t *relay_only_or_absent,
                               const json_t *wake_url, const stz2_seams_t *s);
static json_t *stz2_is_idle(const json_t *active_work_count,
                            const json_t *seconds_since_last_inbound,
                            const json_t *idle_timeout_seconds,
                            const json_t *has_live_background_work, const stz2_seams_t *s);
static json_t *stz2_dashboard_client_heartbeat_path(const json_t *hermes_home,
                                                    const stz2_seams_t *s);
static json_t *stz2_touch_dashboard_client_heartbeat(const json_t *path,
                                                     const stz2_seams_t *s);
static json_t *stz2_dashboard_client_last_seen(const json_t *path, const json_t *now,
                                               const stz2_seams_t *s);

/* PoP: stz2_scale_to_zero_enabled @ gateway/scale_to_zero.py:scale_to_zero_enabled */
static json_t *stz2_scale_to_zero_enabled(const json_t *env, const stz2_seams_t *s) {
    (void)s;
    json_t *v = stz2_env_str(env, "HERMES_SCALE_TO_ZERO", s);
    if (stz2_raised(v)) return v;
    char *low = stz2_strval(v) ? strdup(stz2_strval(v)) : strdup("");
    json_free(v);
    stz2_lower(low);
    bool on = low && (strcmp(low, "1") == 0 || strcmp(low, "true") == 0
                      || strcmp(low, "yes") == 0 || strcmp(low, "on") == 0);
    free(low);
    return json_bool(on);
}

/* PoP: stz2_parse_idle_timeout_seconds @ gateway/scale_to_zero.py:parse_idle_timeout_seconds */
static json_t *stz2_parse_idle_timeout_seconds(const json_t *cfg_value,
                                               const json_t *default_minutes,
                                               const stz2_seams_t *s) {
    (void)s;
    /* try: minutes = float(cfg_value) except (TypeError, ValueError): 0.0 */
    const char *kind = NULL; char *msg = NULL;
    double minutes = stz2_py_float(cfg_value, &kind, &msg);
    if (kind) {
        if (stz2_is_typeerror(kind) || stz2_is_valueerror(kind)) {
            free(msg);
            minutes = 0.0;
        } else {
            json_t *r = stz2_raise(kind, msg);
            free(msg);
            return r;
        }
    }
    /* (float(default_minutes) if minutes <= 0 else minutes) * 60.0
     * — the default branch is CONDITIONAL (lazy). */
    double chosen = minutes;
    if (minutes <= 0.0) {
        const char *dk = NULL; char *dm = NULL;
        chosen = stz2_py_float(default_minutes, &dk, &dm);
        if (dk) {
            json_t *r = stz2_raise(dk, dm);
            free(dm);
            return r;
        }
    }
    return json_number(chosen * 60.0);
}

/* PoP: stz2_messaging_is_relay_only_or_absent @ gateway/scale_to_zero.py:messaging_is_relay_only_or_absent */
static json_t *stz2_messaging_is_relay_only_or_absent(const json_t *platforms,
                                                      const stz2_seams_t *s) {
    (void)s;
    /* names = {str(getattr(p, "value", p)).strip().lower() for p in platforms} */
    if (!platforms || (!stz2_is_obj(platforms) && platforms->type != JSON_ARRAY
                       && platforms->type != JSON_STRING)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable", stz2_typename(platforms));
        return stz2_raise("TypeError", buf);
    }
    char **names = NULL;
    size_t nnames = 0, cap = 0;
    bool failed = false;
    json_t *fail_raise = NULL;
    size_t iter_n = 0;
    if (platforms->type == JSON_ARRAY) iter_n = platforms->c.count;
    else if (platforms->type == JSON_STRING) iter_n = strlen(stz2_strval(platforms));
    else iter_n = platforms->c.count; /* object: keys */
    for (size_t i = 0; i < iter_n && !failed; i++) {
        json_t *p_owned = NULL;
        const json_t *p;
        if (platforms->type == JSON_ARRAY) p = platforms->c.items[i];
        else if (platforms->type == JSON_STRING) {
            char ch[2] = { stz2_strval(platforms)[i], '\0' };
            p_owned = json_string(ch);
            p = p_owned;
        } else {
            p_owned = json_string(platforms->c.keys[i]);
            p = p_owned;
        }
        /* getattr(p, "value", p): object elements are SimpleNamespace doubles */
        const json_t *v;
        char *ns_repr = NULL;
        json_t *ns_node = NULL;
        if (stz2_is_obj(p)) {
            const json_t *val = json_obj_get(p, "value");
            if (val) v = val;
            else {
                ns_repr = stz2_namespace_repr(p);
                ns_node = json_string(ns_repr ? ns_repr : "");
                v = ns_node;
            }
        } else v = p;
        char *raw = stz2_pystr(v);
        char *stripped = stz2_strip(raw);
        free(raw);
        stz2_lower(stripped);
        /* set insert (dedup) */
        bool dup = false;
        for (size_t j = 0; j < nnames; j++)
            if (strcmp(names[j], stripped ? stripped : "") == 0) { dup = true; break; }
        if (!dup) {
            if (nnames == cap) {
                cap = cap ? cap * 2 : 8;
                char **nn = (char **)realloc(names, cap * sizeof(char *));
                if (!nn) { failed = true; fail_raise = stz2_raise("MemoryError", ""); }
                else names = nn;
            }
            if (!failed) names[nnames++] = stripped ? stripped : strdup("");
            else free(stripped);
        } else free(stripped);
        if (ns_repr) free(ns_repr);
        if (ns_node) json_free(ns_node);
        if (p_owned) json_free(p_owned);
    }
    bool only_relay = true;
    for (size_t j = 0; j < nnames; j++) {
        if (strcmp(names[j], "relay") != 0) only_relay = false;
        free(names[j]);
    }
    free(names);
    if (failed) return fail_raise;
    return json_bool(only_relay);
}

/* PoP: stz2_should_arm @ gateway/scale_to_zero.py:should_arm */
static json_t *stz2_should_arm(const json_t *enabled, const json_t *relay_only_or_absent,
                               const json_t *wake_url, const stz2_seams_t *s) {
    (void)s;
    return json_bool(stz2_truth(enabled) && stz2_truth(relay_only_or_absent)
                     && stz2_truth(wake_url));
}

/* PoP: stz2_is_idle @ gateway/scale_to_zero.py:is_idle */
static json_t *stz2_is_idle(const json_t *active_work_count,
                            const json_t *seconds_since_last_inbound,
                            const json_t *idle_timeout_seconds,
                            const json_t *has_live_background_work, const stz2_seams_t *s) {
    (void)s;
    json_t *le = stz2_le_zero(active_work_count);
    if (stz2_raised(le)) return le;
    if (!stz2_truth(le)) { json_free(le); return json_bool(false); }
    json_free(le);
    if (stz2_truth(has_live_background_work)) return json_bool(false);
    return stz2_ge(seconds_since_last_inbound, idle_timeout_seconds);
}

/* PoP: stz2_dashboard_client_heartbeat_path @ gateway/scale_to_zero.py:dashboard_client_heartbeat_path */
static json_t *stz2_dashboard_client_heartbeat_path(const json_t *hermes_home,
                                                    const stz2_seams_t *s) {
    json_t *home = NULL;
    bool home_owned = false;
    if (stz2_is_null(hermes_home)) {
        /* from hermes_constants import get_process_hermes_home */
        json_t *imp = stz2_call(s, "import_get_process_hermes_home", stz2_args());
        if (stz2_raised(imp)) return imp;
        json_free(imp);
        home = stz2_call(s, "get_process_hermes_home", stz2_args());
        home_owned = true;
        if (stz2_raised(home)) return home;
    } else {
        home = json_copy(hermes_home);
        home_owned = true;
    }
    if (!stz2_is_str(home)) {
        char buf[224];
        snprintf(buf, sizeof(buf),
                 "argument should be a str or an os.PathLike object where"
                 " __fspath__ returns a str, not '%s'", stz2_typename(home));
        json_t *r = stz2_raise("TypeError", buf);
        if (home_owned) json_free(home);
        return r;
    }
    char *norm = stz2_path_norm(stz2_strval(home));
    char *joined = stz2_path_join_rel(norm);
    free(norm);
    json_t *r = json_string(joined ? joined : "");
    free(joined);
    if (home_owned) json_free(home);
    return r;
}

/* PoP: stz2_touch_dashboard_client_heartbeat @ gateway/scale_to_zero.py:touch_dashboard_client_heartbeat */
static json_t *stz2_touch_dashboard_client_heartbeat(const json_t *path,
                                                     const stz2_seams_t *s) {
    json_t *p = NULL;
    json_t *caught = NULL;
    if (!stz2_is_null(path)) {
        p = json_copy(path);
    } else {
        p = stz2_dashboard_client_heartbeat_path(NULL, s);
        if (stz2_raised(p)) {
            if (!stz2_is_exception(stz2_kind(p))) return p;  /* BaseException passes */
            caught = p;
            p = NULL;
        }
    }
    if (!caught) {
        /* os.makedirs(os.path.dirname(p), exist_ok=True) — dirname first */
        json_t *a = stz2_args();
        stz2_j(a, "path", p);
        json_t *dir = stz2_call(s, "os.path.dirname", a);
        if (stz2_raised(dir)) {
            if (!stz2_is_exception(stz2_kind(dir))) { json_free(p); return dir; }
            caught = dir;
        } else {
            a = stz2_args();
            json_set(a, "path", dir ? dir : json_null()); /* adopts dir; null -> None */
            json_set(a, "exist_ok", json_bool(true));
            json_t *mr = stz2_call(s, "os.makedirs", a);
            if (stz2_raised(mr)) {
                if (!stz2_is_exception(stz2_kind(mr))) { json_free(p); return mr; }
                caught = mr;
            } else {
                json_free(mr);
                /* open(p, "a", encoding="utf-8").close() */
                a = stz2_args();
                stz2_j(a, "path", p);
                stz2_s(a, "mode", "a");
                stz2_s(a, "encoding", "utf-8");
                json_t *fr = stz2_call(s, "open", a);
                if (stz2_raised(fr)) {
                    if (!stz2_is_exception(stz2_kind(fr))) { json_free(p); return fr; }
                    caught = fr;
                } else if (!stz2_is_obj(fr)) {
                    /* .close() attribute lookup fails on a non-handle */
                    json_t *ar = stz2_attr_raise(fr, "close");
                    json_free(fr);
                    caught = ar;   /* AttributeError is an Exception */
                } else {
                    json_free(fr);
                    json_t *cr = stz2_call(s, "close", stz2_args());
                    if (stz2_raised(cr)) {
                        if (!stz2_is_exception(stz2_kind(cr))) { json_free(p); return cr; }
                        caught = cr;
                    } else {
                        json_free(cr);
                        /* os.utime(p, None) */
                        a = stz2_args();
                        stz2_j(a, "path", p);
                        json_set(a, "times", json_null());
                        json_t *ur = stz2_call(s, "os.utime", a);
                        if (stz2_raised(ur)) {
                            if (!stz2_is_exception(stz2_kind(ur))) { json_free(p); return ur; }
                            caught = ur;
                        } else {
                            json_free(ur);
                            json_free(p);
                            return json_bool(true);
                        }
                    }
                }
            }
        }
    }
    /* except Exception: logger.debug(..., exc_info=True); return False */
    json_free(p);
    json_t *a = stz2_args();
    stz2_s(a, "fmt", "scale-to-zero: dashboard heartbeat touch failed");
    json_set(a, "exc_info", json_bool(true));
    json_t *lr = stz2_call(s, "logger.debug", a);
    if (stz2_raised(lr)) {
        if (caught) json_free(caught);
        return lr;
    }
    json_free(lr);
    if (caught) json_free(caught);
    return json_bool(false);
}

/* PoP: stz2_dashboard_client_last_seen @ gateway/scale_to_zero.py:dashboard_client_last_seen */
static json_t *stz2_dashboard_client_last_seen(const json_t *path, const json_t *now,
                                               const stz2_seams_t *s) {
    /* current = time.time() if now is None else now  [OUTSIDE try] */
    json_t *current;
    if (stz2_is_null(now)) {
        current = stz2_call(s, "time.time", stz2_args());
        if (stz2_raised(current)) return current;
    } else {
        current = json_copy(now);
    }
    /* p = dashboard_client_heartbeat_path() if path is None else path [OUTSIDE try] */
    json_t *p;
    if (!stz2_is_null(path)) {
        p = json_copy(path);
    } else {
        p = stz2_dashboard_client_heartbeat_path(NULL, s);
        if (stz2_raised(p)) { json_free(current); return p; }
    }
    /* try: return min(os.stat(p).st_mtime, current) */
    json_t *a = stz2_args();
    json_set(a, "path", p); /* adopts p */
    json_t *st = stz2_call(s, "os.stat", a);
    if (stz2_raised(st)) {
        const char *k = stz2_kind(st);
        if (stz2_is_filenotfound(k)) {
            json_free(st); json_free(current);
            return json_null();
        }
        if (stz2_is_oserror(k)) {
            json_free(st);
            return current;
        }
        json_free(current);
        return st;
    }
    if (!stz2_is_obj(st)) {
        json_t *r = stz2_attr_raise(st, "st_mtime");
        json_free(st); json_free(current);
        return r;
    }
    const json_t *mtime = json_obj_get(st, "st_mtime");
    if (!mtime) {
        json_t *r = stz2_raise("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'st_mtime'");
        json_free(st); json_free(current);
        return r;
    }
    json_t *r = stz2_min2(mtime, current);
    json_free(st); json_free(current);
    return r;
}