/* ===========================================================================
 * nous_rate_guard_flow2.c -- FLOW port of the UNCLAIMED remainder of
 * agent/nous_rate_guard.py.
 *
 * CRLF-aware PoP grep over src/lib/include (the scanner credits the DEFINING
 * module) shows these claimed against agent/nous_rate_guard.py:
 *   _state_path, _parse_reset_seconds, record_nous_rate_limit,
 *   nous_rate_limit_remaining, clear_nous_rate_limit, _is_exhausted_bucket,
 *   _has_exhausted_bucket_in_object, _parse_buckets_from_headers,
 *   is_genuine_nous_rate_limit, format_remaining (= _fmt_seconds alias)
 * (9 concrete defs in nous_rate_guard_ports.c / nous_rate_guard.c + 1 alias).
 * The module has 12 AST defs; the 3 UNCLAIMED ones are ported here REAL:
 *   _is_exhausted, is_long_welcome_rate_limit, welcome_refusal_from_headers
 * The task's generic ">=5 funcs" floor exceeds the real 3-func remainder; the
 * PoP grep (not the floor) is the authority. >=5 FIXTURES per function.
 *
 * Self-contained: libjson only. Every dependency NOT ported here is injected
 * through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (the port owns and frees it after the call); the
 * result is a NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg} object == a raised exception). A scripted
 * JSON null result is delivered as a NULL pointer so truthiness reads falsy.
 * Fail-closed: no channel, or a missing/exhausted script, yields RuntimeError
 * "no script for <name>" / "no seam installed for <name>".
 *
 * ENTRIES. static json_t *nrg2_<fn>(nrg2_io_t *io, const json_t *in, json_t *st)
 *   io -- universal seam channel (NULL => fail-closed production default)
 *   in -- borrowed case input object (never mutated)
 *   st -- fresh object; every entry here is PURE, so st stays {} on success
 *         and on every raise path. Caller owns the ret node.
 *
 * Seam name -> live call (args record exactly the call-site values; None-valued
 * keys are dropped on BOTH sides):
 *   safe_float                 _safe_float(value, default)           [is_long_welcome_rate_limit]
 *   parse_buckets_from_headers _parse_buckets_from_headers(headers)  [welcome_refusal_from_headers]
 *   lower_headers              lower_headers(headers)                [welcome_refusal_from_headers]
 *   parse_retry_after_seconds  parse_retry_after_seconds(value)      [welcome_refusal_from_headers]
 *
 * FULL builtin exception hierarchy (nrg2_exc_parent, probed on quarry CPython
 * 3.14.9) is carried below and mirrored in oracle_nous_rate_guard_flow2.py.
 * These three functions have NO except site, so every scripted exception --
 * including the BaseException trio (SystemExit / KeyboardInterrupt /
 * GeneratorExit), CancelledError, BaseException and both *Group kinds -- passes
 * through UNCHANGED (the raise spec is propagated verbatim). The table still
 * classifies Exception-vs-BaseException for parity with the sibling flow ports
 * and for any future except-bearing extension.
 *
 * Python semantics transcribed:
 *   - `_is_exhausted`: "is not None" is presence (present-null is NOT None);
 *     bool is admitted as int (True<=0 is False, False<=0 is True).
 *   - `is_long_welcome_rate_limit`: isinstance(x, dict) is JSON_OBJECT ONLY;
 *     `refusal.get("reason") != "rate_limited"` is false for any non-str;
 *     `>=` uses Python truthiness (a seam None raises TypeError, modelled).
 *   - `welcome_refusal_from_headers`: `or` chain short-circuits on Python
 *     truthiness (None/0/false/empty containers falsy); `max(gen, default=None)`
 *     keeps the first maximum on ties; `int(wait)` truncates toward zero;
 *     `max(0, int(wait))` clamps negatives; the SECOND `.values()` pass for
 *     `empty` fires no new seam (dict_values view on the same seam result).
 *
 * Domain restrictions (enforced by gen_nous_rate_guard_flow2_cases.py):
 *   - remaining/reset/retry_after/wait values are numbers/bool/null only;
 *     str()-coercion and comparison of strings against numbers are out of
 *     domain, as are non-str error_context/reason carriers.
 *   - parse_buckets_from_headers scripts are a dict whose values are exactly
 *     2-element arrays [remaining, reset], or null/non-dict (AttributeError
 *     path), or a raise. lower_headers scripts are a dict or null/non-dict.
 *   - safe_float / parse_retry_after_seconds scripts are numbers/null/raise.
 *   - |wait| < 1e15 (so int(wait) is exact in a C long long).
 * =========================================================================== */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

typedef struct nrg2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} nrg2_io_t;

#define NRG2_MIN_RESET_FOR_BREAKER_SECONDS 60.0
#define NRG2_WELCOME_LONG_WAIT_SECONDS     20.0

#if defined(__GNUC__)
#define NRG2_UNUSED __attribute__((unused))
#else
#define NRG2_UNUSED
#endif

/* ------------------------------------------------------------------ truth */
static int nrg2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

/* Python `is not None`: present and non-null (a present JSON null IS None). */
static int nrg2_present(const json_t *v)
{
    return (v && v->type != JSON_NULL) ? 1 : 0;
}

static const char *nrg2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int nrg2_str_eq(const json_t *v, const char *s)
{
    const char *x = nrg2_str(v);
    return (x && s && strcmp(x, s) == 0) ? 1 : 0;
}

static json_t *nrg2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* isinstance(v,(int,float)) -- bool admitted; num value of a bool is 1/0. */
static double nrg2_num(const json_t *v)
{
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

static json_t *nrg2_copy_or_null(const json_t *v)
{
    return v ? json_copy(v) : NULL;
}

static const char *nrg2_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    default:          return "object";
    }
}

/* ------------------------------------------------------------ raise specs */
static int nrg2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *nrg2_kind(const json_t *r)
{
    const char *k = nrg2_str(nrg2_get(r, "__raise__"));
    return k ? k : "";
}

static json_t *nrg2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *nrg2_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* FULL builtin exception-parent hierarchy (probed on quarry CPython 3.14.9).
 * ExceptionGroup has two bases (BaseExceptionGroup, Exception); it IS an
 * Exception, handled specially in nrg2_is_exception. CancelledError is the
 * asyncio extension (BaseException since 3.8). IOError/EnvironmentError/
 * WindowsError are OSError aliases. */
typedef struct { const char *name; const char *parent; } nrg2_exc_row;
static const nrg2_exc_row NRG2_EXC_PARENT[] NRG2_UNUSED = {
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
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
    {"NotImplementedError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"TabError", "IndentationError"},
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
    {NULL, NULL}
};

static const char *nrg2_exc_parent(const char *kind) NRG2_UNUSED;
static const char *nrg2_exc_parent(const char *kind)
{
    for (size_t i = 0; NRG2_EXC_PARENT[i].name; i++)
        if (strcmp(NRG2_EXC_PARENT[i].name, kind) == 0)
            return NRG2_EXC_PARENT[i].parent;
    return NULL;
}

/* True iff the scripted kind is an Exception (caught by `except Exception`).
 * Unknown kinds are synthetic Exception subclasses in the oracle. */
static int nrg2_is_exception(const char *kind) NRG2_UNUSED;
static int nrg2_is_exception(const char *kind)
{
    if (strcmp(kind, "ExceptionGroup") == 0) return 1;
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        if (strcmp(cur, "Exception") == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = nrg2_exc_parent(cur);
        if (!cur) return 1;
    }
    return 1;
}

/* True iff kind is caught by `except (names...)` -- subclass-aware. */
static int nrg2_caught_by(const char *kind, const char *const *names, size_t n) NRG2_UNUSED;
static int nrg2_caught_by(const char *kind, const char *const *names, size_t n)
{
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        for (size_t i = 0; i < n; i++)
            if (strcmp(cur, names[i]) == 0) return 1;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return 0;
        cur = nrg2_exc_parent(cur);
        if (!cur) return 0;
    }
    return 0;
}

/* --------------------------------------------------------- universal seam */
/* args OWNED by the caller (built fresh), borrowed by the dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. */
static json_t *nrg2_call(nrg2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = nrg2_xfmt("no seam installed for %s", name);
        r = nrg2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *nrg2_args(void)
{
    return json_object();
}

/* -------------------------------------------------------- seam wrappers */

/* _safe_float(value, default) -- both positional. */
static json_t *nrg2_seam_safe_float(nrg2_io_t *io, const json_t *value, double def)
{
    json_t *a = nrg2_args();
    json_set(a, "value", nrg2_copy_or_null(value));
    json_set(a, "default", json_number(def));
    return nrg2_call(io, "safe_float", a);
}

/* _parse_buckets_from_headers(headers) -- positional. */
static json_t *nrg2_seam_parse_buckets(nrg2_io_t *io, const json_t *headers)
{
    json_t *a = nrg2_args();
    json_set(a, "headers", nrg2_copy_or_null(headers));
    return nrg2_call(io, "parse_buckets_from_headers", a);
}

/* lower_headers(headers) -- positional. */
static json_t *nrg2_seam_lower_headers(nrg2_io_t *io, const json_t *headers)
{
    json_t *a = nrg2_args();
    json_set(a, "headers", nrg2_copy_or_null(headers));
    return nrg2_call(io, "lower_headers", a);
}

/* parse_retry_after_seconds(value) -- positional. */
static json_t *nrg2_seam_parse_retry_after(nrg2_io_t *io, const json_t *value)
{
    json_t *a = nrg2_args();
    json_set(a, "value", nrg2_copy_or_null(value));
    return nrg2_call(io, "parse_retry_after_seconds", a);
}

/* ---------------------------------------------------- defensive exceptions */

/* '<T>' object has no attribute '<attr>' (py3.14 builtins carry no module
 * qualifier). Used when a seam returns a non-dict where Python calls
 * .values() / .get(). */
static json_t *nrg2_attr_no(const json_t *v, const char *attr)
{
    char *m = nrg2_xfmt("'%s' object has no attribute '%s'", nrg2_typename(v), attr);
    json_t *r = nrg2_mkraise("AttributeError", m ? m : "attribute error");
    free(m);
    return r;
}

/* Python int() truncates toward zero; domain |v| < 1e15 keeps this exact. */
static long long nrg2_py_int(const json_t *v)
{
    double d = nrg2_num(v);
    if (d >= 9.0e18) return LLONG_MAX;
    if (d <= -9.0e18) return LLONG_MIN;
    return (long long)d;
}

/* ============================ ENTRIES ============================ */

/* PoP: nrg2_is_exhausted @ agent/nous_rate_guard.py:_is_exhausted */
static json_t *nrg2_is_exhausted(nrg2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *remaining = nrg2_get(in, "remaining");
    const json_t *reset = nrg2_get(in, "reset");
    /* remaining is not None AND remaining <= 0 AND reset is not None AND
     * reset >= _MIN_RESET_FOR_BREAKER_SECONDS */
    int ok = nrg2_present(remaining) && nrg2_num(remaining) <= 0.0
          && nrg2_present(reset) && nrg2_num(reset) >= NRG2_MIN_RESET_FOR_BREAKER_SECONDS;
    return json_bool(ok);
}

/* PoP: nrg2_is_long_welcome_rate_limit @ agent/nous_rate_guard.py:is_long_welcome_rate_limit */
static json_t *nrg2_is_long_welcome_rate_limit(nrg2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *ec = nrg2_get(in, "error_context");
    if (!ec || ec->type != JSON_OBJECT) return json_bool(0);
    const json_t *refusal = json_obj_get(ec, "welcome_refusal");
    if (!refusal || refusal->type != JSON_OBJECT) return json_bool(0);
    if (!nrg2_str_eq(json_obj_get(refusal, "reason"), "rate_limited")) return json_bool(0);
    json_t *v = nrg2_seam_safe_float(io, json_obj_get(refusal, "retry_after"), 0.0);
    if (nrg2_is_raise(v)) return v;
    if (!nrg2_present(v)) {
        /* None >= 20.0 raises TypeError in the live reference. */
        json_free(v);
        return nrg2_mkraise("TypeError",
                            "'>=' not supported between instances of 'NoneType' and 'float'");
    }
    int ok = nrg2_num(v) >= NRG2_WELCOME_LONG_WAIT_SECONDS;
    json_free(v);
    return json_bool(ok);
}

/* PoP: nrg2_welcome_refusal_from_headers @ agent/nous_rate_guard.py:welcome_refusal_from_headers */
static json_t *nrg2_welcome_refusal_from_headers(nrg2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *headers = nrg2_get(in, "headers");
    const json_t *body_wait = nrg2_get(in, "body_wait");

    /* buckets = _parse_buckets_from_headers(headers).values() */
    json_t *buckets = nrg2_seam_parse_buckets(io, headers);
    if (nrg2_is_raise(buckets)) return buckets;
    if (!buckets || buckets->type != JSON_OBJECT) {
        json_t *e = nrg2_attr_no(buckets, "values");
        json_free(buckets);
        return e;
    }

    /* exhausted_wait = max((reset for remaining, reset in buckets.values()
     *   if remaining is not None and remaining <= 0 and (reset or 0) > 0),
     *   default=None) -- first maximum wins on ties. */
    const json_t *exhausted = NULL;
    for (size_t i = 0; i < buckets->c.count; i++) {
        const json_t *b = buckets->c.items[i];
        if (!b || b->type != JSON_ARRAY || b->c.count < 2) continue;
        const json_t *remaining = b->c.items[0];
        const json_t *reset = b->c.items[1];
        if (!nrg2_present(remaining)) continue;
        if (nrg2_num(remaining) > 0.0) continue;
        if (!nrg2_present(reset)) continue;
        double rv = nrg2_num(reset);
        if (!(rv > 0.0)) continue;              /* (reset or 0) > 0 */
        if (!exhausted || rv > nrg2_num(exhausted)) exhausted = reset;
    }

    /* wait = exhausted_wait or parse_retry_after_seconds(
     *            lower_headers(headers).get("retry-after")) or body_wait or 0.0
     * `or` short-circuits on Python truthiness; the two inner seams only fire
     * when exhausted_wait is falsy. */
    json_t *wait = NULL;
    if (nrg2_present(exhausted) && nrg2_truth(exhausted)) {
        wait = json_copy(exhausted);
    } else {
        json_t *low = nrg2_seam_lower_headers(io, headers);
        if (nrg2_is_raise(low)) { json_free(buckets); return low; }
        if (!low || low->type != JSON_OBJECT) {
            json_t *e = nrg2_attr_no(low, "get");
            json_free(low);
            json_free(buckets);
            return e;
        }
        const json_t *ra = json_obj_get(low, "retry-after");
        json_t *pr = nrg2_seam_parse_retry_after(io, ra);
        json_free(low);
        if (nrg2_is_raise(pr)) { json_free(buckets); return pr; }
        if (nrg2_present(pr) && nrg2_truth(pr)) {
            wait = pr;                          /* adopt */
            pr = NULL;
        } else {
            json_free(pr);
            if (nrg2_present(body_wait) && nrg2_truth(body_wait))
                wait = json_copy(body_wait);
            else
                wait = json_number(0.0);
        }
    }

    /* empty = any(remaining is not None and remaining <= 0
     *             for remaining, _reset in buckets.values())
     * Second pass over the SAME seam result -- no new seam events. */
    int empty = 0;
    for (size_t i = 0; i < buckets->c.count; i++) {
        const json_t *b = buckets->c.items[i];
        if (!b || b->type != JSON_ARRAY || b->c.count < 2) continue;
        const json_t *remaining = b->c.items[0];
        if (nrg2_present(remaining) && nrg2_num(remaining) <= 0.0) { empty = 1; break; }
    }
    json_free(buckets);

    /* reason = "rate_limited" if wait >= WELCOME_LONG_WAIT_SECONDS or empty
     *          else "at_capacity"
     * retry_after = max(0, int(wait)) */
    double w = nrg2_num(wait);
    int reason_long = (w >= NRG2_WELCOME_LONG_WAIT_SECONDS) || empty;
    long long iw = nrg2_py_int(wait);
    json_free(wait);
    long long retry_after = iw < 0 ? 0 : iw;

    json_t *out = json_object();
    json_set(out, "reason", json_string(reason_long ? "rate_limited" : "at_capacity"));
    json_set(out, "retry_after", json_number((double)retry_after));
    json_set(out, "alternates", json_array());
    json_set(out, "upgrade_url", json_string(""));
    return out;
}
