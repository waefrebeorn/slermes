/* delegate_progress_flow2.c — FLOW port of the UNCLAIMED remainder of
 * tools/delegate_tool_progress.py (the 6 progress defs _quiet/_safe_progress/
 * describe_subagent_failure/_format_duration/_print_completion_line/_short
 * were claimed by tool_dispatch_flow2; _clean_error_text/format_subagent_failure_line/
 * format_batch_tag/_batch_prefix by delegate_tool_gaps; _emit_parent_console/
 * _build_child_progress_callback by delegate_tool; _build_child_system_prompt/
 * _resolve_workspace_hint by delegate). This port takes the remaining 12
 * unclaimed defs:
 *
 *   _normalize_event, child_task_message,
 *   _ChildProgressRelay.{__init__, _identity_kwargs, _prefix, _tree_line,
 *     _flush, _on_spawn_requested, _on_text, _on_start, _on_thinking}
 *
 * Claimed siblings are SEAMED (never re-ported): _quiet (CM), _short,
 * _batch_prefix, register_stable_prefix (foreign I/O), the relay's own
 * _relay/_prefix/_tree_line methods when composed by a caller. Ported siblings
 * are driven directly; each extracted method's collaborators are recorders fed
 * the SAME scripts the harness feeds, so the ordered trace matches.
 *
 * Decision sequences only: Python truthiness / or-short-circuits, identity,
 * dict insertion order, str.strip / f-string assembly, and except-Exception
 * catch scopes are transcribed per call site. A scripted BaseException
 * (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError) propagates
 * past every except Exception guard (FULL builtin exception-parent table,
 * audited against the py3.14 gate interpreter).
 *
 * Value model (shared with tests/oracle_delegate_progress_flow2.py):
 *   - attribute doubles {"__attr__":{...}} <-> SimpleNamespace; state window
 *     keeps the wrapper and is mutated in place. __id__ models identity.
 *   - callables project to {"__callable__":"<seam>"}; raise specs
 *     {"__raise__":"<Class>","message":str(e)}.
 *   - DelegateEvent (a str Enum) projects to its .value string.
 *
 * Domain (documented for the generator): ASCII fixture text; the live emoji /
 * box-drawing literals are hex-escaped UTF-8; _short text is ASCII; batch
 * members are strings; context is str-or-null; session_id/delegation_id are
 * strings; toolsets is an array-or-null; integral floats are excluded in
 * str()-computed positions.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified "tools/delegate_progress_flow2.c".
 * Every symbol is static dp2_*.
 *
 * Gate: bash tests/run_one_oracle.sh delegate_progress_flow2 \
 *           oracle_delegate_progress_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam ── */
typedef struct dp2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} dp2_io_t;

/* ── truthiness / accessors ── */
static int dp2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int dp2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int dp2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int dp2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int dp2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int dp2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int dp2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *dp2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *dp2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double dp2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* isinstance(x, dict): a PLAIN object, never an __attr__ double. */
static int dp2_is_plain_dict(const json_t *v)
{
    return dp2_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

/* isinstance(x, (int, float)): a number OR a bool (bool is an int subclass). */
static int dp2_is_number_or_bool(const json_t *v)
{
    return dp2_is_num(v) || dp2_is_bool(v);
}

/* isinstance(x, int): number OR bool; integral floats read as int in libjson. */
static int dp2_is_int(const json_t *v)
{
    if (dp2_is_bool(v)) return 1;
    if (!dp2_is_num(v)) return 0;
    double d = v->num_val;
    return isfinite(d) && d == (double)(long long)d;
}

static const char *dp2_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: {
            const char *tag = dp2_str(dp2_get(v, "__class__"));
            if (tag) return tag;
            if (json_obj_get(v, "__attr__")) return "types.SimpleNamespace";
            if (json_obj_get(v, "__children__")) return "list";
            if (json_obj_get(v, "__event__")) return "threading.Event";
            return "dict";
        }
        default: return "object";
    }
}

/* ── attribute doubles ── */
static json_t *dp2_dattr(const json_t *d, const char *name)
{
    return dp2_get(dp2_get(d, "__attr__"), name);
}

/* getattr(d, name, default): a wrapper reads __attr__.name; None / non-wrapper
 * yields the default (getattr never raises when a default is given). */
static json_t *dp2_getattr(const json_t *d, const char *name, json_t *defval)
{
    json_t *inner = dp2_get(d, "__attr__");
    if (inner) {
        json_t *v = json_obj_get(inner, name);
        return v ? v : defval;
    }
    return defval;
}

/* hasattr(d, name): a wrapper carries the key in __attr__. */
static int dp2_hasattr(const json_t *d, const char *name)
{
    json_t *inner = dp2_get(d, "__attr__");
    return inner && json_obj_get(inner, name) != NULL;
}

static void dp2_dset(const json_t *d, const char *name, json_t *val)
{
    json_t *inner = dp2_get(d, "__attr__");
    if (inner && inner->type == JSON_OBJECT) json_set(inner, name, val);
    else json_free(val);
}

/* identity: two doubles are the same object iff their __id__ strings match. */
static int dp2_same_id(const json_t *a, const json_t *b)
{
    const char *ia = dp2_str(dp2_get(a, "__id__"));
    const char *ib = dp2_str(dp2_get(b, "__id__"));
    if (ia && ib) return strcmp(ia, ib) == 0;
    return a == b;
}

/* ── raise specs ── */
static int dp2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *dp2_kind(const json_t *r)
{
    const char *k = dp2_str(dp2_get(r, "__raise__"));
    return k ? k : "";
}

static const char *dp2_raise_msg(const json_t *r)
{
    const char *m = dp2_str(dp2_get(r, "message"));
    return m ? m : "";
}

static json_t *dp2_mkraise(const char *kind, const char *msg)
{
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}

/* ── FULL builtin exception-parent hierarchy (py3.14, audited against the gate
 *    interpreter's builtins). Walk to test except Exception / except <Class>
 *    ancestry. The BaseException trio (+ asyncio.CancelledError) parent to
 *    BaseException ONLY, so they pass through every except Exception. ── */
typedef struct { const char *name; const char *parent; } dp2_exc_row;
static const dp2_exc_row DP2_EXC_PARENTS[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"CancelledError", "BaseException"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"EnvironmentError", "OSError"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"IOError", "OSError"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "OSError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
    {NULL, NULL},
};

static const char *dp2_exc_parent(const char *name)
{
    for (size_t i = 0; DP2_EXC_PARENTS[i].name; i++)
        if (strcmp(DP2_EXC_PARENTS[i].name, name) == 0) return DP2_EXC_PARENTS[i].parent;
    return "Exception";
}

static int dp2_exc_is_instance(const char *kind, const char *base)
{
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, base) == 0) return 1;
        const char *p = dp2_exc_parent(cur);
        if (!p || strcmp(cur, p) == 0) break;
        snprintf(cur, sizeof(cur), "%s", p);
        if (strcmp(cur, "BaseException") == 0) return strcmp(base, "BaseException") == 0;
    }
    return 0;
}

static int dp2_exc_is_exception(const char *kind)
{
    return dp2_exc_is_instance(kind, "Exception");
}

/* ── printf-style malloc'd format ── */
static char *dp2_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { va_end(ap2); return NULL; }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

/* ── universal seam call (args owned by this wrapper, freed after delegating;
 *    result owned by the caller). No seam installed -> fail-closed RuntimeError. ── */
static json_t *dp2_call(dp2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = dp2_xfmt("no seam installed for %s", name);
        r = dp2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *dp2_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides. */
static void dp2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void dp2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void dp2_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void dp2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* double projection for a seam arg: copy the wrapper verbatim. */
static json_t *dp2_double_proj(const json_t *v)
{
    return v ? json_copy(v) : json_null();
}
/* ── string helpers (ASCII domain) ── */
static json_t *dp2_strn(const char *s, size_t n)
{
    char *tmp = (char *)malloc(n + 1);
    if (!tmp) return json_string("");
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    json_t *r = json_string(tmp);
    free(tmp);
    return r;
}

/* shortest round-trip decimal of a non-integral double (Python float repr). */
static char *dp2_float_repr(double d)
{
    char buf[64];
    for (int p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*g", p, d);
        char *end = NULL;
        double back = strtod(buf, &end);
        if (end && *end == '\0' && back == d) return dp2_xfmt("%s", buf);
    }
    snprintf(buf, sizeof(buf), "%.17g", d);
    return dp2_xfmt("%s", buf);
}

/* Python str() projection (domain: str / number / bool / None). */
static char *dp2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return dp2_xfmt("None");
    if (v->type == JSON_BOOL) return dp2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return dp2_xfmt("%s", v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18)
            return dp2_xfmt("%.0f", d);
        return dp2_float_repr(d);
    }
    return dp2_xfmt("");
}

/* Python s[:n] slice (n >= 0; past-the-end yields the whole string). */
static json_t *dp2_slice(const json_t *s, size_t n)
{
    const char *p = dp2_str(s);
    if (!p) return json_string("");
    size_t len = strlen(p);
    if (n >= len) return json_string(p);
    return dp2_strn(p, n);
}

/* count '%' chars (live log_message.count("%")). */
static int dp2_count_pct(const char *s)
{
    int n = 0;
    for (const char *p = s; p && *p; p++) if (*p == '%') n++;
    return n;
}

/* Python round(x) -> integer, half-to-even (banker's rounding). */
static long long dp2_round_int(double x)
{
    if (!isfinite(x)) return 0;
    double f = floor(x);
    double diff = x - f;
    if (diff > 0.5) return (long long)f + 1;
    if (diff < 0.5) return (long long)f;
    /* exact tie: pick the even neighbour */
    long long fi = (long long)f;
    return (fi % 2 == 0) ? fi : fi + 1;
}

/* case-insensitive ASCII prefix test. */
static int dp2_iprefix(const char *s, const char *pref)
{
    while (*pref) {
        char a = *s, b = *pref;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
        s++; pref++;
    }
    return 1;
}

/* float() grammar validator on a whitespace-stripped string (no underscores;
 * hex floats rejected). Returns 1 when the whole string is a valid float. */
static int dp2_float_grammar(const char *s)
{
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    if (dp2_iprefix(p, "inf")) {
        p += 3;
        if (dp2_iprefix(p, "inity")) p += 5;
        return *p == '\0';
    }
    if (dp2_iprefix(p, "nan")) { p += 3; return *p == '\0'; }
    int digits = 0;
    while (isdigit((unsigned char)*p)) { p++; digits = 1; }
    if (*p == '.') { p++; while (isdigit((unsigned char)*p)) { p++; digits = 1; } }
    if (!digits) return 0;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        int ed = 0;
        while (isdigit((unsigned char)*p)) { p++; ed = 1; }
        if (!ed) return 0;
    }
    return *p == '\0';
}

/* Python float(v): a number node on success, else a TypeError/ValueError raise
 * spec. Domain: numbers, bools, ASCII numeric strings, inf/nan; null/list/dict
 * -> TypeError; non-numeric str -> ValueError (repr-quoted, ASCII printable). */
static json_t *dp2_py_float(const json_t *v)
{
    if (dp2_is_num(v)) return json_number(v->num_val);
    if (dp2_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (dp2_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        /* strip ASCII whitespace both ends */
        const char *a = s;
        while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\v' || *a == '\f') a++;
        const char *b = s + strlen(s);
        while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' || b[-1] == '\r' || b[-1] == '\v' || b[-1] == '\f')) b--;
        size_t n = (size_t)(b - a);
        char *clean = (char *)malloc(n + 1);
        if (!clean) return dp2_mkraise("ValueError", "could not convert string to float");
        memcpy(clean, a, n); clean[n] = '\0';
        if (n == 0 || !dp2_float_grammar(clean)) {
            char *m = dp2_xfmt("could not convert string to float: '%s'", clean);
            json_t *r = dp2_mkraise("ValueError", m ? m : "could not convert string to float");
            free(m); free(clean);
            return r;
        }
        double d = strtod(clean, NULL);
        free(clean);
        return json_number(d);
    }
    char *m = dp2_xfmt("float() argument must be a string or a real number, not '%s'", dp2_typename(v));
    json_t *r = dp2_mkraise("TypeError", m ? m : "float() argument");
    free(m);
    return r;
}

/* ── logger seam (args recorded VERBATIM; %s/%r apply only at emission) ── */
static json_t *dp2_logger(dp2_io_t *io, const char *level, const char *msg, json_t *args_arr, int exc_info)
{
    json_t *a = dp2_args();
    dp2_arg_s(a, "msg", msg);
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) dp2_arg_b(a, "exc_info", 1);
    char *nm = dp2_xfmt("logger.%s", level);
    json_t *r = dp2_call(io, nm ? nm : "logger.info", a);
    free(nm);
    return r;
}

/* _quiet exit: swallow an Exception raise; a non-None log_message is logged at
 * debug with the exception filling a trailing unsatisfied %s (count('%') >
 * len(log_args)). log_args is BORROWED. Returns NULL when swallowed, or the
 * logger.debug raise spec if that call itself raises. A BaseException is NOT
 * swallowed (returned for the caller to propagate). */
static json_t *dp2_quiet_swallow(dp2_io_t *io, const char *msg, const json_t *log_args, int exc_info, json_t *r)
{
    if (!dp2_exc_is_exception(dp2_kind(r))) return r;  /* BaseException passes */
    if (!msg) { json_free(r); return NULL; }
    int pct = dp2_count_pct(msg);
    size_t nargs = dp2_is_arr(log_args) ? json_len(log_args) : 0;
    json_t *la = json_array();
    for (size_t i = 0; i < nargs; i++) json_append(la, json_copy(json_get(log_args, i)));
    if (pct > (int)nargs) json_append(la, json_string(dp2_raise_msg(r)));
    json_free(r);
    return dp2_logger(io, "debug", msg, la, exc_info);
}

/* AttributeError raise spec for a missing attribute on a carrier. */
static json_t *dp2_attr_raise(const json_t *obj, const char *name)
{
    char *m = dp2_xfmt("'%s' object has no attribute '%s'", dp2_typename(obj), name);
    json_t *r = dp2_mkraise("AttributeError", m ? m : "attribute error");
    free(m);
    return r;
}

/* =====================================================================
 * tools/delegate_tool_progress.py — UNCLAIMED remainder
 * ===================================================================== */

/* strip ASCII whitespace into a fresh NUL-terminated buffer (Python
 * str.strip over the documented ASCII domain). */
static char *dp2_strip(const char *s)
{
    static const char *WS = " \t\n\r\v\f";
    if (!s) s = "";
    const char *a = s;
    while (*a && strchr(WS, *a)) a++;
    const char *b = s + strlen(s);
    while (b > a && strchr(WS, b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return dp2_xfmt("");
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* first truthy operand of (a or b or "") over strings. */
static const char *dp2_or_str(const char *a, const char *b)
{
    if (a && *a) return a;
    if (b && *b) return b;
    return "";
}

/* PoP: dp2_normalize_event @ tools/delegate_tool_progress.py:_normalize_event */
static json_t *dp2_normalize_event(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *s = dp2_str(dp2_get(in, "event_type"));
    if (!s) return json_null(); /* non-str: dict.get miss -> DelegateEvent(v) ValueError -> None */
    static const char *LIFE[] = {"subagent.spawn_requested", "subagent.start",
                                 "subagent.complete", "subagent.text", NULL};
    static const char *LEG[] = {"_thinking", "reasoning.available", "tool.started",
                                "tool.completed", "subagent_progress", NULL};
    static const char *LEGVAL[] = {"delegate.task_thinking", "delegate.task_thinking",
                                   "delegate.tool_started", "delegate.tool_completed",
                                   "delegate.task_progress"};
    static const char *DELEG[] = {"delegate.task_progress", "delegate.task_thinking",
                                  "delegate.tool_started", "delegate.tool_completed", NULL};
    for (int i = 0; LIFE[i]; i++) if (strcmp(s, LIFE[i]) == 0) return json_string(s);
    for (int i = 0; LEG[i]; i++) if (strcmp(s, LEG[i]) == 0) return json_string(LEGVAL[i]);
    for (int i = 0; DELEG[i]; i++) if (strcmp(s, DELEG[i]) == 0) return json_string(DELEG[i]);
    return json_null();
}

/* PoP: dp2_child_task_message @ tools/delegate_tool_progress.py:child_task_message */
static json_t *dp2_child_task_message(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *goal = dp2_str(dp2_get(in, "goal"));
    if (!goal) goal = "";
    const char *ctx = dp2_str(dp2_get(in, "context"));
    if (!ctx) return json_string(goal);
    char *trimmed = dp2_strip(ctx);
    if (!*trimmed) { free(trimmed); return json_string(goal); }
    free(trimmed);
    /* live uses RAW context in the f-string; strip only gates the early return */
    char *prefix = dp2_xfmt("CONTEXT:\n%s\n\nTASK:\n", ctx);
    json_t *pa = dp2_args();
    dp2_arg_s(pa, "prefix", prefix);
    json_t *r = dp2_call(io, "register_stable_prefix", pa);
    if (dp2_is_raise(r)) { free(prefix); return r; }
    json_free(r);
    char *ret = dp2_xfmt("%s%s", prefix, goal);
    free(prefix);
    json_t *out = json_string(ret);
    free(ret);
    return out;
}

/* PoP: dp2_relay_init @ tools/delegate_tool_progress.py:_ChildProgressRelay.__init__ */
static json_t *dp2_relay_init(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *goal = dp2_str(dp2_get(in, "goal"));
    char *gl = dp2_strip(goal);
    json_t *relay = json_object();
    json_set(relay, "__attr__", json_object());
    json_t *at = dp2_get(relay, "__attr__");
    json_set(at, "task_index", dp2_get(in, "task_index") ? json_copy(dp2_get(in, "task_index")) : json_null());
    json_set(at, "task_count", dp2_get(in, "task_count") ? json_copy(dp2_get(in, "task_count")) : json_null());
    json_set(at, "goal_label", json_string(gl));
    free(gl);
    json_set(at, "spinner", dp2_get(in, "spinner") ? json_copy(dp2_get(in, "spinner")) : json_null());
    json_set(at, "parent_cb", dp2_get(in, "parent_cb") ? json_copy(dp2_get(in, "parent_cb")) : json_null());
    json_t *sr = dp2_get(in, "session_ref");
    json_set(at, "session_ref", (sr && sr->type != JSON_NULL) ? json_copy(sr) : json_object());
    const char *keys[5] = {"subagent_id", "parent_id", "depth", "model", "toolsets"};
    for (int i = 0; i < 5; i++)
        json_set(at, keys[i], dp2_get(in, keys[i]) ? json_copy(dp2_get(in, keys[i])) : json_null());
    json_set(at, "batch", json_array());
    json_set(at, "parent_scope", json_null());
    json_set(at, "tool_count", json_number(0));
    json_set(st, "relay", relay);
    return json_null();
}

/* shared: the self double's __attr__ from the state window. */
static json_t *dp2_self_attrs(json_t *st)
{
    return dp2_get(dp2_get(st, "relay"), "__attr__");
}

/* PoP: dp2_identity_kwargs @ tools/delegate_tool_progress.py:_ChildProgressRelay._identity_kwargs */
static json_t *dp2_identity_kwargs(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    json_t *at = dp2_self_attrs(st);
    json_t *kw = json_object();
    static const char *base[3] = {"task_index", "task_count", "goal_label"};
    static const char *bdst[3] = {"task_index", "task_count", "goal"};
    for (int i = 0; i < 3; i++) {
        json_t *v = dp2_get(at, base[i]);
        json_set(kw, bdst[i], v ? json_copy(v) : json_null());
    }
    static const char *opt[4] = {"subagent_id", "parent_id", "depth", "model"};
    for (int i = 0; i < 4; i++) {
        json_t *v = dp2_get(at, opt[i]);
        if (v && v->type != JSON_NULL) json_set(kw, opt[i], json_copy(v));
    }
    json_t *ts = dp2_get(at, "toolsets");
    if (ts && ts->type != JSON_NULL) json_set(kw, "toolsets", json_copy(ts));
    json_t *sr = dp2_get(at, "session_ref");
    static const char *srcs[2] = {"session_id", "delegation_id"};
    static const char *dsts[2] = {"child_session_id", "delegation_id"};
    for (int i = 0; i < 2; i++) {
        json_t *v = dp2_get(sr, srcs[i]);
        if (dp2_truth(v)) {
            char *s = dp2_pystr(v);
            json_set(kw, dsts[i], json_string(s ? s : ""));
            free(s);
        }
    }
    json_t *tc = dp2_get(at, "tool_count");
    json_set(kw, "tool_count", tc ? json_copy(tc) : json_null());
    return kw;
}

/* PoP: dp2_prefix @ tools/delegate_tool_progress.py:_ChildProgressRelay._prefix */
static json_t *dp2_prefix(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *at = dp2_self_attrs(st);
    json_t *sr = dp2_get(at, "session_ref");
    json_t *a = dp2_args();
    dp2_arg_j(a, "delegation_id", dp2_get(sr, "delegation_id"));
    json_t *ti = dp2_get(at, "task_index");
    json_t *tc = dp2_get(at, "task_count");
    json_set(a, "task_index", ti ? json_copy(ti) : json_null());
    json_set(a, "task_count", tc ? json_copy(tc) : json_null());
    dp2_arg_j(a, "parent_agent", dp2_get(sr, "_parent_scope"));
    return dp2_call(io, "batch_prefix", a);
}

/* PoP: dp2_tree_line @ tools/delegate_tool_progress.py:_ChildProgressRelay._tree_line */
static json_t *dp2_tree_line(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *at = dp2_self_attrs(st);
    json_t *spinner = dp2_get(at, "spinner");
    if (!dp2_truth(spinner)) return json_null();
    /* Python resolves the OUTER attribute self.spinner.print_above BEFORE
     * evaluating the f-string argument self._prefix(). A missing attribute
     * raises here with NO prefix seam event. */
    json_t *pa = dp2_dattr(spinner, "print_above");
    json_t *body_raise = NULL;
    if (!pa) {
        body_raise = dp2_attr_raise(spinner, "print_above");
    } else {
        json_t *pref = dp2_call(io, "prefix", dp2_args());
        if (dp2_is_raise(pref)) {
            body_raise = pref;
        } else {
            const char *ps = dp2_str(pref); if (!ps) ps = "";
            const char *text = dp2_str(dp2_get(in, "text")); if (!text) text = "";
            char *line = dp2_xfmt(" %s\xe2\x94\x9c\xe2\x94\x80 %s", ps, text);
            if (dp2_is_obj(pa) && json_obj_get(pa, "__callable__")) {
                const char *seam = dp2_str(dp2_get(pa, "__callable__"));
                json_t *a = dp2_args();
                dp2_arg_s(a, "line", line);
                json_t *r = dp2_call(io, seam ? seam : "spinner.print_above", a);
                if (dp2_is_raise(r)) body_raise = r; else json_free(r);
            } else {
                char *m = dp2_xfmt("'%s' object is not callable", dp2_typename(pa));
                body_raise = dp2_mkraise("TypeError", m ? m : "not callable");
                free(m);
            }
            free(line);
            json_free(pref);
        }
    }
    if (body_raise) {
        json_t *sw = dp2_quiet_swallow(io, "Spinner print_above failed: %s", NULL, 0, body_raise);
        if (sw) return sw;
    }
    return json_null();
}

/* PoP: dp2_flush @ tools/delegate_tool_progress.py:_ChildProgressRelay._flush */
static json_t *dp2_flush(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *at = dp2_self_attrs(st);
    json_t *pcb = dp2_get(at, "parent_cb");
    json_t *batch = dp2_get(at, "batch");
    if (!dp2_truth(pcb) || !dp2_is_arr(batch) || json_len(batch) == 0) return json_null();
    /* preview f-string evaluated BEFORE the _relay call: prefix seam first. */
    json_t *pref = dp2_call(io, "prefix", dp2_args());
    if (dp2_is_raise(pref)) return pref;
    const char *ps = dp2_str(pref); if (!ps) ps = "";
    /* ", ".join(batch): members are strings in the documented domain. */
    size_t cap = 1, used = 0;
    for (size_t i = 0; i < json_len(batch); i++) {
        const char *v = dp2_str(json_get(batch, i));
        cap += (v ? strlen(v) : 4) + 2;
    }
    char *joined = (char *)malloc(cap);
    if (!joined) joined = (char *)malloc(1);
    joined[0] = '\0';
    for (size_t i = 0; i < json_len(batch); i++) {
        if (i) { strcat(joined, ", "); used += 2; }
        const char *v = dp2_str(json_get(batch, i));
        if (v) { strcat(joined, v); used += strlen(v); }
    }
    (void)used;
    char *prev = dp2_xfmt("\xf0\x9f\x94\x80 %s%s", ps, joined);
    free(joined);
    json_free(pref);
    json_t *a = dp2_args();
    dp2_arg_s(a, "event_type", "subagent.progress");
    dp2_arg_s(a, "preview", prev);
    json_set(a, "kwargs", json_object());
    free(prev);
    json_t *r = dp2_call(io, "relay", a);
    if (dp2_is_raise(r)) return r;
    json_free(r);
    json_set(at, "batch", json_array()); /* self.batch.clear() */
    return json_null();
}

/* Build the standard relay seam args: event_type + optional preview + kwargs. */
static json_t *dp2_relay_args(const char *event_type, const char *preview,
                              const json_t *kwargs, int use_preview)
{
    json_t *a = dp2_args();
    dp2_arg_s(a, "event_type", event_type);
    if (use_preview) dp2_arg_s(a, "preview", preview);
    json_set(a, "kwargs", (kwargs && kwargs->type == JSON_OBJECT) ? json_copy(kwargs) : json_object());
    return a;
}

/* PoP: dp2_on_spawn_requested @ tools/delegate_tool_progress.py:_ChildProgressRelay._on_spawn_requested */
static json_t *dp2_on_spawn_requested(dp2_io_t *io, const json_t *in, json_t *st)
{
    json_t *at = dp2_self_attrs(st);
    const char *gl = dp2_str(dp2_get(at, "goal_label"));
    const char *pv = dp2_or_str(dp2_str(dp2_get(in, "preview")), gl);
    return dp2_call(io, "relay", dp2_relay_args("subagent.spawn_requested", pv, dp2_get(in, "kwargs"), 1));
}

/* PoP: dp2_on_text @ tools/delegate_tool_progress.py:_ChildProgressRelay._on_text */
static json_t *dp2_on_text(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *a = dp2_args();
    dp2_arg_s(a, "event_type", "subagent.text");
    dp2_arg_j(a, "preview", dp2_get(in, "preview"));
    json_set(a, "kwargs", json_object()); /* live passes no **kwargs here */
    return dp2_call(io, "relay", a);
}

/* PoP: dp2_on_start @ tools/delegate_tool_progress.py:_ChildProgressRelay._on_start */
static json_t *dp2_on_start(dp2_io_t *io, const json_t *in, json_t *st)
{
    json_t *at = dp2_self_attrs(st);
    const char *gl = dp2_str(dp2_get(at, "goal_label"));
    if (gl && *gl) {
        json_t *sa = dp2_args();
        dp2_arg_s(sa, "text", gl);
        dp2_arg_n(sa, "n", 55);
        json_t *sh = dp2_call(io, "short", sa);
        if (dp2_is_raise(sh)) return sh;
        const char *sv = dp2_str(sh); if (!sv) sv = "";
        char *line = dp2_xfmt("\xf0\x9f\x94\x80 %s", sv);
        json_t *ta = dp2_args();
        dp2_arg_s(ta, "line", line);
        json_t *tr = dp2_call(io, "tree_line", ta);
        free(line);
        if (dp2_is_raise(tr)) { json_free(sh); return tr; }
        json_free(tr);
        json_free(sh);
    }
    const char *pv = dp2_or_str(dp2_str(dp2_get(in, "preview")), gl);
    return dp2_call(io, "relay", dp2_relay_args("subagent.start", pv, dp2_get(in, "kwargs"), 1));
}

/* PoP: dp2_on_thinking @ tools/delegate_tool_progress.py:_ChildProgressRelay._on_thinking */
static json_t *dp2_on_thinking(dp2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *text = dp2_or_str(dp2_str(dp2_get(in, "preview")), dp2_str(dp2_get(in, "tool_name")));
    json_t *sa = dp2_args();
    dp2_arg_s(sa, "text", text);
    dp2_arg_n(sa, "n", 55);
    json_t *sh = dp2_call(io, "short", sa);
    if (dp2_is_raise(sh)) return sh;
    const char *sv = dp2_str(sh); if (!sv) sv = "";
    char *line = dp2_xfmt("\xf0\x9f\x92\xad \"%s\"", sv);
    json_t *ta = dp2_args();
    dp2_arg_s(ta, "line", line);
    json_t *tr = dp2_call(io, "tree_line", ta);
    free(line);
    if (dp2_is_raise(tr)) { json_free(sh); return tr; }
    json_free(tr);
    json_t *a = dp2_args();
    dp2_arg_s(a, "event_type", "subagent.thinking");
    dp2_arg_s(a, "preview", text);
    json_set(a, "kwargs", json_object()); /* live passes no **kwargs here */
    json_free(sh);
    return dp2_call(io, "relay", a);
}
