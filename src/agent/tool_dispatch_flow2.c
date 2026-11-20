/* tool_dispatch_flow2.c — FLOW port of the UNCLAIMED remainder of the
 * delegate_task tool-dispatch surface. The task label "agent/tool_dispatch.py"
 * does not exist in the quarry; the live dispatch core is
 * tools/delegate_tool_dispatch.py (fully claimed by tool_dispatch_flow / 21
 * funcs + clarify_tool.c _run_batch). This port takes the REMAINING unclaimed
 * collaborator functions that flow1 seamed, across their defining modules:
 *
 *   tools/delegate_tool_child_run.py (12): _num, _str_or_none, _tier_in_effect,
 *     _route_report, _fabricated_entry, _append_missed_steer, _close_child,
 *     _with_children_lock, _attach_child, _detach_child, _signal_child_stop,
 *     _child_activity_fingerprint
 *   tools/delegate_tool_progress.py (6): _quiet, _safe_progress,
 *     describe_subagent_failure, _format_duration, _print_completion_line, _short
 *   tools/delegate_tool_prefix_gate.py (6): _route_key, _caches_prompts,
 *     _first_response_started, _stop_requested, PrefixCacheGate.for_children,
 *     PrefixCacheGate.run (the class __init__/follows/_wait machinery is
 *     transcribed inline as td2_gate_* helpers, no separate PoP)
 *   tools/delegate_tool_config.py (1): _get_prefix_stagger_seconds (_knob
 *     transcribed inline as td2_knob; _load_config is a foreign-I/O seam)
 *   tools/delegate_tool_auto_resume.py (1): waiting_on_resume
 *   gateway/session_finite.py (1): finite_turn_required
 *   tools/process_registry_notifications.py (1): _process_accounting_lines
 *
 * Claimed siblings are SEAMED (never re-ported): _clean_error_text,
 * _emit_parent_console (progress), request_hard_interrupt (interrupt_compat).
 * Foreign unclaimed leaves are seamed too: service_tier_word (fast_mode),
 * failure_cause_gloss (turn_failure_copy), _load_config (config I/O). Ported
 * siblings compose for REAL (_str_or_none/_tier_in_effect inside _route_report,
 * _route_report inside _fabricated_entry, _with_children_lock inside
 * _attach_child/_detach_child, _signal_child_stop inside _attach_child,
 * _quiet logic inside _close_child/_safe_progress/_print_completion_line/
 * _signal_child_stop/_attach_child, _route_key/_caches_prompts inside the gate,
 * _first_response_started/_stop_requested inside gate._wait).
 *
 * Decision sequences only: Python truthiness, or/and short-circuits, is None /
 * is True identity vs truthy tests, kwarg/operand evaluation order, dict
 * insertion order, and except-Exception / except-(ValueError,UnboundLocalError)
 * catch scopes are transcribed per call site. A scripted BaseException
 * (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError) propagates
 * past every except Exception guard exactly as live (FULL builtin exception
 * parent table below, audited against the py3.14 gate interpreter).
 *
 * Value model (shared with tests/oracle_tool_dispatch_flow2.py):
 *   - attribute doubles {"__attr__": {...}, "__id__": "c0"?, "__class__": tag?}
 *     <-> SimpleNamespace; the state window keeps the wrapper form and is
 *     mutated in place. __id__ models object identity (gate leader / children
 *     list membership); the oracle interns by it so `is` agrees.
 *   - the parent's _active_children is a {"__children__": [child doubles]}
 *     marker <-> a real list; the active_children_op seam models list.append /
 *     list.remove deterministically (remove-absent raises ValueError).
 *   - events (threading.Event) are {"__event__": "h1"} markers; is_set / wait /
 *     set ride seams or state flags as documented per call site.
 *   - callables project to {"__callable__": "<seam>"}; raise specs
 *     {"__raise__": "<ClassName>", "message": str(e)}.
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - fixture text is ASCII; the live em-dash (U+2014) literal in
 *     _process_accounting_lines is hex-escaped.
 *   - _format_duration / _get_prefix_stagger_seconds avoid integral floats in
 *     str()-computed positions where the live result is a float string; round()
 *     fixtures use banker's rounding (probe-verified).
 *   - float() coercion in _get_prefix_stagger_seconds covers numbers, bools,
 *     ASCII numeric strings, and raising forms (null/list -> TypeError,
 *     non-numeric str -> ValueError); exotic underscore/inf/nan strings are
 *     outside the fixture corpus.
 *   - _process_accounting_lines / _child_activity_fingerprint carriers are
 *     lists-or-null / dict-or-null (a truthy non-container is outside the
 *     documented domain).
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified "agent/tool_dispatch_flow2.c". Every
 * symbol is static td2_*.
 *
 * Gate: bash tests/run_one_oracle.sh tool_dispatch_flow2 \
 *           oracle_tool_dispatch_flow2.py "" "lib/libjson/json.o"
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

/* ── live module constants (byte-pinned) ── */
/* delegate_tool_prefix_gate._FIRST_CHUNK_ACTIVITY */
#define TD2_FIRST_CHUNK "receiving stream response"
/* delegate_tool_prefix_gate._POLL_S */
#define TD2_POLL_S 0.05
/* U+2014 em dash (process_registry_notifications f-strings). */
#define TD2_EMDASH "\xe2\x80\x94"

/* ── universal seam ── */
typedef struct td2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} td2_io_t;

/* ── truthiness / accessors ── */
static int td2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int td2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int td2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int td2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int td2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int td2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int td2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *td2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *td2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double td2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* isinstance(x, dict): a PLAIN object, never an __attr__ double. */
static int td2_is_plain_dict(const json_t *v)
{
    return td2_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

/* isinstance(x, (int, float)): a number OR a bool (bool is an int subclass). */
static int td2_is_number_or_bool(const json_t *v)
{
    return td2_is_num(v) || td2_is_bool(v);
}

/* isinstance(x, int): number OR bool; integral floats read as int in libjson. */
static int td2_is_int(const json_t *v)
{
    if (td2_is_bool(v)) return 1;
    if (!td2_is_num(v)) return 0;
    double d = v->num_val;
    return isfinite(d) && d == (double)(long long)d;
}

static const char *td2_typename(const json_t *v)
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
            const char *tag = td2_str(td2_get(v, "__class__"));
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
static json_t *td2_dattr(const json_t *d, const char *name)
{
    return td2_get(td2_get(d, "__attr__"), name);
}

/* getattr(d, name, default): a wrapper reads __attr__.name; None / non-wrapper
 * yields the default (getattr never raises when a default is given). */
static json_t *td2_getattr(const json_t *d, const char *name, json_t *defval)
{
    json_t *inner = td2_get(d, "__attr__");
    if (inner) {
        json_t *v = json_obj_get(inner, name);
        return v ? v : defval;
    }
    return defval;
}

/* hasattr(d, name): a wrapper carries the key in __attr__. */
static int td2_hasattr(const json_t *d, const char *name)
{
    json_t *inner = td2_get(d, "__attr__");
    return inner && json_obj_get(inner, name) != NULL;
}

static void td2_dset(const json_t *d, const char *name, json_t *val)
{
    json_t *inner = td2_get(d, "__attr__");
    if (inner && inner->type == JSON_OBJECT) json_set(inner, name, val);
    else json_free(val);
}

/* identity: two doubles are the same object iff their __id__ strings match. */
static int td2_same_id(const json_t *a, const json_t *b)
{
    const char *ia = td2_str(td2_get(a, "__id__"));
    const char *ib = td2_str(td2_get(b, "__id__"));
    if (ia && ib) return strcmp(ia, ib) == 0;
    return a == b;
}

/* ── raise specs ── */
static int td2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *td2_kind(const json_t *r)
{
    const char *k = td2_str(td2_get(r, "__raise__"));
    return k ? k : "";
}

static const char *td2_raise_msg(const json_t *r)
{
    const char *m = td2_str(td2_get(r, "message"));
    return m ? m : "";
}

static json_t *td2_mkraise(const char *kind, const char *msg)
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
typedef struct { const char *name; const char *parent; } td2_exc_row;
static const td2_exc_row TD2_EXC_PARENTS[] = {
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

static const char *td2_exc_parent(const char *name)
{
    for (size_t i = 0; TD2_EXC_PARENTS[i].name; i++)
        if (strcmp(TD2_EXC_PARENTS[i].name, name) == 0) return TD2_EXC_PARENTS[i].parent;
    return "Exception";
}

static int td2_exc_is_instance(const char *kind, const char *base)
{
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, base) == 0) return 1;
        const char *p = td2_exc_parent(cur);
        if (!p || strcmp(cur, p) == 0) break;
        snprintf(cur, sizeof(cur), "%s", p);
        if (strcmp(cur, "BaseException") == 0) return strcmp(base, "BaseException") == 0;
    }
    return 0;
}

static int td2_exc_is_exception(const char *kind)
{
    return td2_exc_is_instance(kind, "Exception");
}

/* ── printf-style malloc'd format ── */
static char *td2_xfmt(const char *fmt, ...)
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
static json_t *td2_call(td2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = td2_xfmt("no seam installed for %s", name);
        r = td2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *td2_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides. */
static void td2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void td2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void td2_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void td2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* double projection for a seam arg: copy the wrapper verbatim. */
static json_t *td2_double_proj(const json_t *v)
{
    return v ? json_copy(v) : json_null();
}
/* ── string helpers (ASCII domain) ── */
static json_t *td2_strn(const char *s, size_t n)
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
static char *td2_float_repr(double d)
{
    char buf[64];
    for (int p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*g", p, d);
        char *end = NULL;
        double back = strtod(buf, &end);
        if (end && *end == '\0' && back == d) return td2_xfmt("%s", buf);
    }
    snprintf(buf, sizeof(buf), "%.17g", d);
    return td2_xfmt("%s", buf);
}

/* Python str() projection (domain: str / number / bool / None). */
static char *td2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return td2_xfmt("None");
    if (v->type == JSON_BOOL) return td2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return td2_xfmt("%s", v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18)
            return td2_xfmt("%.0f", d);
        return td2_float_repr(d);
    }
    return td2_xfmt("");
}

/* Python s[:n] slice (n >= 0; past-the-end yields the whole string). */
static json_t *td2_slice(const json_t *s, size_t n)
{
    const char *p = td2_str(s);
    if (!p) return json_string("");
    size_t len = strlen(p);
    if (n >= len) return json_string(p);
    return td2_strn(p, n);
}

/* count '%' chars (live log_message.count("%")). */
static int td2_count_pct(const char *s)
{
    int n = 0;
    for (const char *p = s; p && *p; p++) if (*p == '%') n++;
    return n;
}

/* Python round(x) -> integer, half-to-even (banker's rounding). */
static long long td2_round_int(double x)
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
static int td2_iprefix(const char *s, const char *pref)
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
static int td2_float_grammar(const char *s)
{
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    if (td2_iprefix(p, "inf")) {
        p += 3;
        if (td2_iprefix(p, "inity")) p += 5;
        return *p == '\0';
    }
    if (td2_iprefix(p, "nan")) { p += 3; return *p == '\0'; }
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
static json_t *td2_py_float(const json_t *v)
{
    if (td2_is_num(v)) return json_number(v->num_val);
    if (td2_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (td2_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        /* strip ASCII whitespace both ends */
        const char *a = s;
        while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\v' || *a == '\f') a++;
        const char *b = s + strlen(s);
        while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' || b[-1] == '\r' || b[-1] == '\v' || b[-1] == '\f')) b--;
        size_t n = (size_t)(b - a);
        char *clean = (char *)malloc(n + 1);
        if (!clean) return td2_mkraise("ValueError", "could not convert string to float");
        memcpy(clean, a, n); clean[n] = '\0';
        if (n == 0 || !td2_float_grammar(clean)) {
            char *m = td2_xfmt("could not convert string to float: '%s'", clean);
            json_t *r = td2_mkraise("ValueError", m ? m : "could not convert string to float");
            free(m); free(clean);
            return r;
        }
        double d = strtod(clean, NULL);
        free(clean);
        return json_number(d);
    }
    char *m = td2_xfmt("float() argument must be a string or a real number, not '%s'", td2_typename(v));
    json_t *r = td2_mkraise("TypeError", m ? m : "float() argument");
    free(m);
    return r;
}

/* ── logger seam (args recorded VERBATIM; %s/%r apply only at emission) ── */
static json_t *td2_logger(td2_io_t *io, const char *level, const char *msg, json_t *args_arr, int exc_info)
{
    json_t *a = td2_args();
    td2_arg_s(a, "msg", msg);
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) td2_arg_b(a, "exc_info", 1);
    char *nm = td2_xfmt("logger.%s", level);
    json_t *r = td2_call(io, nm ? nm : "logger.info", a);
    free(nm);
    return r;
}

/* _quiet exit: swallow an Exception raise; a non-None log_message is logged at
 * debug with the exception filling a trailing unsatisfied %s (count('%') >
 * len(log_args)). log_args is BORROWED. Returns NULL when swallowed, or the
 * logger.debug raise spec if that call itself raises. A BaseException is NOT
 * swallowed (returned for the caller to propagate). */
static json_t *td2_quiet_swallow(td2_io_t *io, const char *msg, const json_t *log_args, int exc_info, json_t *r)
{
    if (!td2_exc_is_exception(td2_kind(r))) return r;  /* BaseException passes */
    if (!msg) { json_free(r); return NULL; }
    int pct = td2_count_pct(msg);
    size_t nargs = td2_is_arr(log_args) ? json_len(log_args) : 0;
    json_t *la = json_array();
    for (size_t i = 0; i < nargs; i++) json_append(la, json_copy(json_get(log_args, i)));
    if (pct > (int)nargs) json_append(la, json_string(td2_raise_msg(r)));
    json_free(r);
    return td2_logger(io, "debug", msg, la, exc_info);
}

/* AttributeError raise spec for a missing attribute on a carrier. */
static json_t *td2_attr_raise(const json_t *obj, const char *name)
{
    char *m = td2_xfmt("'%s' object has no attribute '%s'", td2_typename(obj), name);
    json_t *r = td2_mkraise("AttributeError", m ? m : "attribute error");
    free(m);
    return r;
}
/* =====================================================================
 * tools/delegate_tool_child_run.py
 * ===================================================================== */

/* _str_or_none value helper: str -> copy, else None. */
static json_t *td2_str_or_none_v(const json_t *v)
{
    if (td2_is_str(v)) return json_copy(v);
    return json_null();
}

/* PoP: td2_num @ tools/delegate_tool_child_run.py:_num */
static json_t *td2_num_fn(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *v = td2_get(in, "value");
    json_t *def = td2_get(in, "default");
    double dval = def ? td2_num(def) : 0.0;
    if (td2_is_number_or_bool(v)) {
        double d = td2_is_bool(v) ? (v->bool_val ? 1.0 : 0.0) : v->num_val;
        return json_number((double)(long long)d);  /* int() truncates toward zero */
    }
    return json_number(dval);
}

/* PoP: td2_str_or_none @ tools/delegate_tool_child_run.py:_str_or_none */
static json_t *td2_str_or_none_fn(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return td2_str_or_none_v(td2_get(in, "value"));
}

/* _tier_in_effect value helper (no seams; attribute/dict reads). */
static json_t *td2_tier_in_effect_v(const json_t *child)
{
    json_t *stier = td2_getattr(child, "service_tier", NULL);
    if (td2_truth(stier)) return json_copy(stier);
    json_t *ov = td2_getattr(child, "request_overrides", NULL);
    if (!td2_is_plain_dict(ov)) ov = NULL;
    json_t *eb = td2_get(ov, "extra_body");
    if (!td2_is_plain_dict(eb)) eb = NULL;
    json_t *scopes[2] = {ov, eb};
    for (int i = 0; i < 2; i++) {
        json_t *scope = scopes[i];
        json_t *t = td2_get(scope, "service_tier");
        if (td2_truth(t)) return json_copy(t);
        json_t *sp = td2_get(scope, "speed");
        if (td2_is_str(sp) && strcmp(td2_str(sp), "fast") == 0) return json_string("priority");
    }
    return json_null();
}

/* PoP: td2_tier_in_effect @ tools/delegate_tool_child_run.py:_tier_in_effect */
static json_t *td2_tier_in_effect(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return td2_tier_in_effect_v(td2_get(st, "child"));
}

/* _route_report value helper (composes _str_or_none / _tier_in_effect; the
 * service_tier_word foreign leaf is a seam). May return a raise spec. */
static json_t *td2_route_report_v(td2_io_t *io, const json_t *child)
{
    json_t *route = td2_getattr(child, "_delegate_route", NULL);
    if (!td2_is_plain_dict(route)) return json_null();
    json_t *reasoning = td2_getattr(child, "reasoning_config", NULL);
    json_t *effort_node = json_null();
    if (td2_is_plain_dict(reasoning)) {
        json_t *en = td2_get(reasoning, "enabled");
        int enabled = en ? td2_truth(en) : 1;  /* default True */
        if (enabled) {
            json_t *e = td2_get(reasoning, "effort");
            json_free(effort_node);
            effort_node = e ? json_copy(e) : json_null();
        } else {
            json_free(effort_node);
            effort_node = json_string("none");
        }
    }
    json_t *out = json_copy(route);
    json_set(out, "provider", td2_str_or_none_v(td2_getattr(child, "provider", NULL)));
    json_set(out, "model", td2_str_or_none_v(td2_getattr(child, "model", NULL)));
    json_set(out, "reasoning_effort", td2_str_or_none_v(effort_node));
    json_free(effort_node);
    json_t *tier = td2_tier_in_effect_v(child);
    json_t *sa = td2_args();
    td2_arg_j(sa, "tier", tier);
    json_free(tier);
    json_t *stw = td2_call(io, "service_tier_word", sa);
    if (td2_is_raise(stw)) { json_free(out); return stw; }
    json_set(out, "service_tier", stw ? stw : json_null());
    json_t *fa = td2_getattr(child, "_fallback_activated", NULL);
    if (td2_is_bool(fa) && fa->bool_val) json_set(out, "fallback_activated", json_bool(true));
    return out;
}

/* PoP: td2_route_report @ tools/delegate_tool_child_run.py:_route_report */
static json_t *td2_route_report(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    return td2_route_report_v(io, td2_get(st, "child"));
}

/* PoP: td2_fabricated_entry @ tools/delegate_tool_child_run.py:_fabricated_entry */
static json_t *td2_fabricated_entry(td2_io_t *io, const json_t *in, json_t *st)
{
    json_t *child = td2_get(st, "child");
    json_t *e = json_object();
    json_t *idx = td2_get(in, "idx");
    json_set(e, "task_index", idx ? json_copy(idx) : json_number(0));
    json_t *status = td2_get(in, "status");
    json_set(e, "status", status ? json_copy(status) : json_string(""));
    json_set(e, "summary", json_null());
    json_t *err = td2_get(in, "error");
    json_set(e, "error", err ? json_copy(err) : json_null());
    json_set(e, "api_calls", json_number(0));
    json_t *dur = td2_get(in, "duration");
    json_set(e, "duration_seconds", dur ? json_copy(dur) : json_number(0));
    json_t *role = td2_getattr(child, "_delegate_role", NULL);
    json_set(e, "_child_role", role ? json_copy(role) : json_null());
    json_t *route = td2_route_report_v(io, child);
    if (td2_is_raise(route)) { json_free(e); return route; }
    if (route && route->type != JSON_NULL) json_set(e, "route", route);
    else json_free(route);
    return e;
}

/* PoP: td2_append_missed_steer @ tools/delegate_tool_child_run.py:_append_missed_steer */
static json_t *td2_append_missed_steer(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *entry = td2_get(st, "entry");
    json_t *ls = td2_get(in, "late_steer");
    if (!td2_truth(ls)) return json_null();
    json_set(entry, "missed_steer", json_copy(ls));
    json_t *err = td2_get(entry, "error");
    if (!err) return td2_mkraise("KeyError", "'error'");
    if (!td2_is_str(err)) {
        char *m = td2_xfmt("can only concatenate str (not \"%s\") to str", td2_typename(err));
        json_t *r = td2_mkraise("TypeError", m ? m : "concat");
        free(m);
        return r;
    }
    char *suffix = td2_xfmt(" [steer did not land before the subagent stopped: %s]", td2_str(ls));
    char *newerr = td2_xfmt("%s%s", td2_str(err), suffix ? suffix : "");
    json_set(entry, "error", json_string(newerr ? newerr : ""));
    free(suffix); free(newerr);
    return json_null();
}

/* PoP: td2_close_child @ tools/delegate_tool_child_run.py:_close_child */
static json_t *td2_close_child(td2_io_t *io, const json_t *in, json_t *st)
{
    json_t *child = td2_get(st, "child");
    const char *lm = td2_str(td2_get(in, "log_message"));
    json_t *close = td2_getattr(child, "close", NULL);
    json_t *body_raise = NULL;
    if (close && td2_is_obj(close) && json_obj_get(close, "__callable__")) {
        const char *seam = td2_str(td2_get(close, "__callable__"));
        json_t *r = td2_call(io, seam ? seam : "child.close", td2_args());
        if (td2_is_raise(r)) body_raise = r; else json_free(r);
    }
    /* with _quiet(log_message, exc_info=True) */
    if (body_raise) {
        json_t *sw = td2_quiet_swallow(io, lm, NULL, 1, body_raise);
        if (sw) return sw;
    }
    return json_null();
}

/* _with_children_lock op call helper: the active_children_op seam models
 * list.append / list.remove deterministically (dispatcher special-case). */
static json_t *td2_with_children_lock_op(td2_io_t *io, const json_t *parent, const char *op, const json_t *child)
{
    json_t *ac = td2_dattr(parent, "_active_children");
    if (!ac) return td2_attr_raise(parent, "_active_children");
    if (!op || (strcmp(op, "append") != 0 && strcmp(op, "remove") != 0))
        return td2_attr_raise(ac, op ? op : "");
    json_t *a = td2_args();
    td2_arg_s(a, "op", op);
    td2_arg_j(a, "child", child);
    return td2_call(io, "active_children_op", a);
}

/* PoP: td2_with_children_lock @ tools/delegate_tool_child_run.py:_with_children_lock */
static json_t *td2_with_children_lock(td2_io_t *io, const json_t *in, json_t *st)
{
    json_t *parent = td2_get(st, "parent_agent");
    json_t *child = td2_get(st, "child");
    const char *op = td2_str(td2_get(in, "op"));
    /* lock = getattr(parent, "_active_children_lock", None); if lock: with lock:
     * ... else: ... — the lock CM is silent (single-threaded), both branches
     * perform the identical op call. */
    json_t *r = td2_with_children_lock_op(io, parent, op, child);
    if (td2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* _signal_child_stop value helper (composes request_hard_interrupt seam +
 * setattr; _quiet(None) swallow). reasons is a BORROWED array. */
static json_t *td2_signal_child_stop_v(td2_io_t *io, const json_t *child, const json_t *reasons, const char *tool_reason)
{
    json_t *body_raise = NULL;
    if (!td2_is_null(child)) {
        json_t *a = td2_args();
        td2_arg_j(a, "child", child);
        json_set(a, "reason", reasons ? json_copy(reasons) : json_array());
        td2_arg_s(a, "tool_reason", tool_reason);
        json_t *r = td2_call(io, "request_hard_interrupt", a);
        if (td2_is_raise(r)) body_raise = r;
        else {
            int not_r = !td2_truth(r);
            json_free(r);
            if (not_r && td2_hasattr(child, "_interrupt_requested"))
                td2_dset(child, "_interrupt_requested", json_bool(true));
        }
    }
    if (body_raise) {
        json_t *sw = td2_quiet_swallow(io, NULL, NULL, 0, body_raise);
        if (sw) return sw;
    }
    return json_null();
}

/* PoP: td2_signal_child_stop @ tools/delegate_tool_child_run.py:_signal_child_stop */
static json_t *td2_signal_child_stop(td2_io_t *io, const json_t *in, json_t *st)
{
    json_t *child = td2_get(st, "child");
    json_t *reasons = td2_get(in, "reasons");
    json_t *tr = td2_get(in, "tool_reason");
    const char *tool_reason = td2_is_str(tr) ? td2_str(tr) : "parent delegation ended";
    return td2_signal_child_stop_v(io, child, reasons, tool_reason);
}

/* PoP: td2_attach_child @ tools/delegate_tool_child_run.py:_attach_child */
static json_t *td2_attach_child(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *parent = td2_get(st, "parent_agent");
    json_t *child = td2_get(st, "child");
    if (td2_hasattr(parent, "_active_children")) {
        json_t *r = td2_with_children_lock_op(io, parent, "append", child);
        if (td2_is_raise(r)) return r;
        json_free(r);
    }
    json_t *ir = td2_getattr(parent, "_interrupt_requested", NULL);
    if (!(td2_is_bool(ir) && ir->bool_val)) return json_null();  /* is not True */
    json_t *message = td2_getattr(parent, "_interrupt_message", NULL);
    json_t *hard = td2_getattr(parent, "_hard_interrupt_requested", NULL);
    int hard_is_none = td2_is_null(hard);
    int hard_set = 0;
    if (!hard_is_none) {
        const char *hid = td2_str(td2_get(hard, "__event__"));
        json_t *a = td2_args();
        td2_arg_s(a, "id", hid);
        json_t *r = td2_call(io, "event.is_set", a);
        if (td2_is_raise(r)) return r;
        hard_set = td2_truth(r);
        json_free(r);
    }
    if (hard_is_none || hard_set) {
        const char *reason = td2_truth(message) ? td2_str(message) : "parent agent interrupted";
        json_t *tr = td2_getattr(parent, "_tool_interrupt_reason", NULL);
        const char *tool_reason = td2_truth(tr) ? td2_str(tr) : "parent agent interrupted";
        json_t *reasons = json_array();
        json_append(reasons, json_string(reason ? reason : "parent agent interrupted"));
        json_t *r = td2_signal_child_stop_v(io, child, reasons, tool_reason);
        json_free(reasons);
        if (td2_is_raise(r)) return r;
        json_free(r);
        return json_null();
    }
    /* soft: with _quiet("Failed to propagate interrupt to late child: %s"):
     * child.interrupt(message) */
    json_t *interrupt = td2_dattr(child, "interrupt");
    json_t *body_raise = NULL;
    if (interrupt && td2_is_obj(interrupt) && json_obj_get(interrupt, "__callable__")) {
        const char *seam = td2_str(td2_get(interrupt, "__callable__"));
        json_t *a = td2_args();
        td2_arg_j(a, "message", message);
        json_t *r = td2_call(io, seam ? seam : "child.interrupt", a);
        if (td2_is_raise(r)) body_raise = r; else json_free(r);
    } else if (!interrupt) {
        body_raise = td2_attr_raise(child, "interrupt");
    } else {
        char *m = td2_xfmt("'%s' object is not callable", td2_typename(interrupt));
        body_raise = td2_mkraise("TypeError", m ? m : "not callable");
        free(m);
    }
    if (body_raise) {
        json_t *sw = td2_quiet_swallow(io, "Failed to propagate interrupt to late child: %s", NULL, 0, body_raise);
        if (sw) return sw;
    }
    return json_null();
}

/* PoP: td2_detach_child @ tools/delegate_tool_child_run.py:_detach_child */
static json_t *td2_detach_child(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *parent = td2_get(st, "parent_agent");
    json_t *child = td2_get(st, "child");
    if (!td2_hasattr(parent, "_active_children")) return json_null();
    json_t *r = td2_with_children_lock_op(io, parent, "remove", child);
    if (td2_is_raise(r)) {
        const char *k = td2_kind(r);
        if (td2_exc_is_instance(k, "ValueError") || td2_exc_is_instance(k, "UnboundLocalError")) {
            json_t *la = json_array();
            json_append(la, json_string(td2_raise_msg(r)));
            json_free(r);
            json_t *lr = td2_logger(io, "debug", "Could not remove child from active_children: %s", la, 0);
            if (td2_is_raise(lr)) return lr;
            json_free(lr);
            return json_null();
        }
        return r;  /* other kinds propagate */
    }
    json_free(r);
    return json_null();
}

/* PoP: td2_child_activity_fingerprint @ tools/delegate_tool_child_run.py:_child_activity_fingerprint */
static json_t *td2_child_activity_fingerprint(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *child = td2_get(st, "child");
    (void)child;
    json_t *r = td2_call(io, "child.get_activity_summary", td2_args());
    json_t *triple_null = json_array();
    json_append(triple_null, json_null());
    json_append(triple_null, json_null());
    json_append(triple_null, json_null());
    if (td2_is_raise(r)) {
        if (td2_exc_is_exception(td2_kind(r))) { json_free(r); return triple_null; }
        json_free(triple_null);
        return r;  /* BaseException propagates */
    }
    json_t *summary;
    if (!td2_truth(r)) { json_free(r); summary = json_object(); }
    else summary = r;
    if (!td2_is_plain_dict(summary)) { json_free(summary); return triple_null; }
    json_t *out = json_array();
    const char *keys[3] = {"api_call_count", "current_tool", "last_activity_ts"};
    for (int i = 0; i < 3; i++) {
        json_t *v = td2_get(summary, keys[i]);
        json_append(out, v ? json_copy(v) : json_null());
    }
    json_free(summary);
    json_free(triple_null);
    return out;
}
/* =====================================================================
 * tools/delegate_tool_progress.py
 * ===================================================================== */

/* PoP: td2_quiet @ tools/delegate_tool_progress.py:_quiet
 * Direct drive of the contextmanager: the body is the quiet.body seam; the
 * except-Exception swallow + debug log is transcribed (td2_quiet_swallow). */
static json_t *td2_quiet_fn(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *lm = td2_str(td2_get(in, "log_message"));
    json_t *la = td2_get(in, "log_args");
    int ei = td2_truth(td2_get(in, "exc_info")) ? 1 : 0;
    json_t *r = td2_call(io, "quiet.body", td2_args());
    if (td2_is_raise(r)) {
        json_t *sw = td2_quiet_swallow(io, lm, la, ei, r);
        if (sw) return sw;
        return json_null();
    }
    json_free(r);
    return json_null();
}

/* PoP: td2_safe_progress @ tools/delegate_tool_progress.py:_safe_progress */
static json_t *td2_safe_progress(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *cb = td2_get(in, "cb");
    if (!td2_truth(cb)) return json_null();
    json_t *et = td2_get(in, "event_type");
    json_t *args = td2_get(in, "args");
    json_t *kwargs = td2_get(in, "kwargs");
    json_t *body_raise = NULL;
    if (td2_is_obj(cb) && json_obj_get(cb, "__callable__")) {
        const char *seam = td2_str(td2_get(cb, "__callable__"));
        json_t *a = td2_args();
        td2_arg_j(a, "event_type", et);
        json_set(a, "args", args ? json_copy(args) : json_array());
        json_set(a, "kwargs", kwargs ? json_copy(kwargs) : json_object());
        json_t *r = td2_call(io, seam ? seam : "progress.cb", a);
        if (td2_is_raise(r)) body_raise = r; else json_free(r);
    } else {
        char *m = td2_xfmt("'%s' object is not callable", td2_typename(cb));
        body_raise = td2_mkraise("TypeError", m ? m : "not callable");
        free(m);
    }
    if (body_raise) {
        json_t *la = json_array();
        json_append(la, et ? json_copy(et) : json_null());
        json_t *sw = td2_quiet_swallow(io, "Progress callback %s failed: %s", la, 0, body_raise);
        json_free(la);
        if (sw) return sw;
    }
    return json_null();
}

/* PoP: td2_describe_subagent_failure @ tools/delegate_tool_progress.py:describe_subagent_failure */
static json_t *td2_describe_subagent_failure(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *fr = td2_get(in, "failure_reason");
    json_t *err = td2_get(in, "error");
    json_t *mc = td2_get(in, "max_chars");
    double maxc = mc ? td2_num(mc) : 200.0;
    json_t *a = td2_args();
    td2_arg_j(a, "failure_reason", fr);
    td2_arg_s(a, "subject", "it");
    td2_arg_s(a, "possessive", "its");
    json_t *gloss = td2_call(io, "failure_cause_gloss", a);
    if (td2_is_raise(gloss)) return gloss;
    if (td2_truth(gloss)) return gloss;  /* gloss or ... returns the operand */
    json_free(gloss);
    json_t *ca = td2_args();
    td2_arg_j(ca, "error", err);
    td2_arg_n(ca, "max_chars", maxc);
    return td2_call(io, "clean_error_text", ca);
}

/* PoP: td2_format_duration @ tools/delegate_tool_progress.py:_format_duration */
static json_t *td2_format_duration(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *s = td2_get(in, "seconds");
    if (!td2_is_number_or_bool(s)) return json_string("");
    double d = td2_is_bool(s) ? (s->bool_val ? 1.0 : 0.0) : s->num_val;
    if (d <= 0) return json_string("");
    char *r;
    if (d >= 120) r = td2_xfmt("%lld min", td2_round_int(d / 60.0));
    else r = td2_xfmt("%llds", td2_round_int(d));
    json_t *out = json_string(r ? r : "");
    free(r);
    return out;
}

/* PoP: td2_print_completion_line @ tools/delegate_tool_progress.py:_print_completion_line */
static json_t *td2_print_completion_line(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *spinner = td2_get(in, "spinner");
    const char *ls = td2_str(td2_get(in, "line"));
    json_t *cl = td2_get(in, "console_line");
    if (td2_truth(spinner)) {
        json_t *pa = td2_dattr(spinner, "print_above");
        json_t *body_raise = NULL;
        if (pa && td2_is_obj(pa) && json_obj_get(pa, "__callable__")) {
            const char *seam = td2_str(td2_get(pa, "__callable__"));
            json_t *a = td2_args();
            td2_arg_s(a, "line", ls);
            json_t *r = td2_call(io, seam ? seam : "spinner.print_above", a);
            if (td2_is_raise(r)) body_raise = r; else json_free(r);
        } else if (!pa) {
            body_raise = td2_attr_raise(spinner, "print_above");
        } else {
            char *m = td2_xfmt("'%s' object is not callable", td2_typename(pa));
            body_raise = td2_mkraise("TypeError", m ? m : "not callable");
            free(m);
        }
        if (body_raise) {
            json_t *sw = td2_quiet_swallow(io, NULL, NULL, 0, body_raise);
            if (sw) return sw;
        }
        return json_null();
    }
    char *out_line;
    if (td2_is_null(cl)) out_line = td2_xfmt("  %s", ls ? ls : "");
    else out_line = td2_xfmt("%s", td2_str(cl) ? td2_str(cl) : "");
    json_t *parent = td2_get(in, "parent_agent");
    json_t *a = td2_args();
    td2_arg_j(a, "parent_agent", parent);
    td2_arg_s(a, "line", out_line);
    free(out_line);
    json_t *r = td2_call(io, "emit_parent_console", a);
    if (td2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: td2_short @ tools/delegate_tool_progress.py:_short */
static json_t *td2_short_fn(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *t = td2_str(td2_get(in, "text"));
    if (!t) t = "";
    long long n = (long long)td2_num(td2_get(in, "n"));
    size_t len = strlen(t);
    if (n >= 0 && (long long)len > n) {
        char *r = td2_xfmt("%.*s...", (int)n, t);
        json_t *out = json_string(r ? r : "");
        free(r);
        return out;
    }
    return json_string(t);
}
/* ── JSON deep equality (route-key comparison) ── */
static int td2_json_eq(const json_t *a, const json_t *b)
{
    int na = td2_is_null(a), nb = td2_is_null(b);
    if (na || nb) return na && nb;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_BOOL: return (a->bool_val ? 1 : 0) == (b->bool_val ? 1 : 0);
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY: {
            if (json_len(a) != json_len(b)) return 0;
            for (size_t i = 0; i < json_len(a); i++)
                if (!td2_json_eq(json_get(a, i), json_get(b, i))) return 0;
            return 1;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !td2_json_eq(a->c.items[i], bv)) return 0;
            }
            return 1;
        }
        default: return 0;
    }
}

/* =====================================================================
 * tools/delegate_tool_prefix_gate.py
 * ===================================================================== */

static json_t *td2_route_key_v(const json_t *child)
{
    const char *attrs[4] = {"provider", "base_url", "model", "api_mode"};
    json_t *out = json_array();
    for (int i = 0; i < 4; i++) {
        json_t *v = td2_getattr(child, attrs[i], NULL);
        json_append(out, v ? json_copy(v) : json_null());
    }
    return out;
}

static int td2_caches_prompts_v(const json_t *child)
{
    json_t *v = td2_getattr(child, "_use_prompt_caching", NULL);
    return td2_is_bool(v) && v->bool_val;
}

static int td2_first_response_started_v(const json_t *child)
{
    json_t *sac = td2_getattr(child, "session_api_calls", NULL);
    double v = td2_truth(sac) ? td2_num(sac) : 0.0;  /* (x or 0) */
    if (v >= 1) return 1;
    json_t *desc = td2_getattr(child, "_last_activity_desc", NULL);
    const char *ds = td2_str(desc);
    return ds && strcmp(ds, TD2_FIRST_CHUNK) == 0;
}

static int td2_stop_requested_v(const json_t *child)
{
    json_t *v = td2_getattr(child, "_interrupt_requested", NULL);
    return td2_is_bool(v) && v->bool_val;
}

/* PoP: td2_route_key @ tools/delegate_tool_prefix_gate.py:_route_key */
static json_t *td2_route_key(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return td2_route_key_v(td2_get(st, "child"));
}

/* PoP: td2_caches_prompts @ tools/delegate_tool_prefix_gate.py:_caches_prompts */
static json_t *td2_caches_prompts(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return json_bool(td2_caches_prompts_v(td2_get(st, "child")));
}

/* PoP: td2_first_response_started @ tools/delegate_tool_prefix_gate.py:_first_response_started */
static json_t *td2_first_response_started(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return json_bool(td2_first_response_started_v(td2_get(st, "child")));
}

/* PoP: td2_stop_requested @ tools/delegate_tool_prefix_gate.py:_stop_requested */
static json_t *td2_stop_requested(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return json_bool(td2_stop_requested_v(td2_get(st, "child")));
}

/* PoP: td2_gate_for_children @ tools/delegate_tool_prefix_gate.py:for_children */
static json_t *td2_gate_for_children(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *children = td2_get(in, "children");
    double mw = td2_num(td2_get(in, "max_wait_s"));
    if (mw <= 0) return json_null();
    if (!td2_is_arr(children) || json_len(children) < 2) return json_null();
    json_t *tr0 = json_get(children, 0);
    json_t *leader = td2_is_arr(tr0) ? json_get(tr0, 2) : NULL;
    if (!td2_caches_prompts_v(leader)) return json_null();
    json_t *gate = json_object();
    json_set(gate, "leader", leader ? json_copy(leader) : json_null());
    json_set(gate, "max_wait_s", json_number(mw));
    json_set(gate, "leader_route", td2_route_key_v(leader));
    json_set(gate, "leader_done", json_bool(false));
    json_set(st, "gate", json_copy(gate));
    return gate;
}

/* gate._wait: poll until the leader is done / its first response started, a
 * stop is requested, or the deadline elapses. time.monotonic / event.wait ride
 * seams; leader_done.is_set reads the state flag. Returns json_bool or a raise. */
static json_t *td2_gate_wait(td2_io_t *io, json_t *gate, const json_t *child, const json_t *parent)
{
    json_t *mono0 = td2_call(io, "time.monotonic", td2_args());
    if (td2_is_raise(mono0)) return mono0;
    double deadline = td2_num(mono0) + td2_num(td2_get(gate, "max_wait_s"));
    json_free(mono0);
    json_t *leader = td2_get(gate, "leader");
    for (int iter = 0; iter < 100000; iter++) {
        int done = td2_truth(td2_get(gate, "leader_done")) || td2_first_response_started_v(leader);
        if (done) break;
        if (td2_stop_requested_v(child) || (!td2_is_null(parent) && td2_stop_requested_v(parent)))
            return json_bool(false);
        json_t *mono = td2_call(io, "time.monotonic", td2_args());
        if (td2_is_raise(mono)) return mono;
        double remaining = deadline - td2_num(mono);
        json_free(mono);
        if (remaining <= 0) break;
        double t = remaining < TD2_POLL_S ? remaining : TD2_POLL_S;
        json_t *a = td2_args();
        td2_arg_n(a, "timeout", t);
        json_t *wr = td2_call(io, "event.wait", a);
        if (td2_is_raise(wr)) return wr;
        json_free(wr);
    }
    return json_bool(true);
}

/* gate.follows: child is not leader AND caches prompts AND route matches. */
static int td2_gate_follows(const json_t *gate, const json_t *child)
{
    json_t *leader = td2_get(gate, "leader");
    if (td2_same_id(child, leader)) return 0;
    if (!td2_caches_prompts_v(child)) return 0;
    json_t *rk = td2_route_key_v(child);
    json_t *lr = td2_get(gate, "leader_route");
    int eq = td2_json_eq(rk, lr);
    json_free(rk);
    return eq;
}

/* PoP: td2_gate_run @ tools/delegate_tool_prefix_gate.py:run */
static json_t *td2_gate_run(td2_io_t *io, const json_t *in, json_t *st)
{
    json_t *gate = td2_get(st, "gate");
    json_t *child = td2_get(in, "child");
    json_t *leader = td2_get(gate, "leader");
    json_t *run = td2_get(in, "run");
    json_t *parent = td2_get(in, "parent");
    const char *run_seam = td2_str(td2_get(run, "__callable__"));
    if (!run_seam) run_seam = "gate.run_fn";
    if (td2_same_id(child, leader)) {
        json_t *r = td2_call(io, run_seam, td2_args());
        json_set(gate, "leader_done", json_bool(true));  /* finally: set() */
        if (td2_is_raise(r)) return r;
        return r ? r : json_null();
    }
    if (td2_gate_follows(gate, child)) {
        json_t *w = td2_gate_wait(io, gate, child, parent);
        if (td2_is_raise(w)) return w;
        int waited = td2_truth(w);
        json_free(w);
        if (!waited) return json_null();
    }
    json_t *r = td2_call(io, run_seam, td2_args());
    if (td2_is_raise(r)) return r;
    return r ? r : json_null();
}

/* =====================================================================
 * tools/delegate_tool_config.py — _get_prefix_stagger_seconds (_knob inline)
 * ===================================================================== */

/* PoP: td2_get_prefix_stagger_seconds @ tools/delegate_tool_config.py:_get_prefix_stagger_seconds */
static json_t *td2_get_prefix_stagger_seconds(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *cfg = td2_call(io, "load_config", td2_args());
    if (td2_is_raise(cfg)) return cfg;
    json_t *val = td2_get(cfg, "prefix_stagger_seconds");
    json_t *valc = val ? json_copy(val) : NULL;
    json_free(cfg);
    if (valc && valc->type != JSON_NULL) {
        json_t *f = td2_py_float(valc);
        if (td2_is_raise(f)) {
            const char *k = td2_kind(f);
            if (td2_exc_is_instance(k, "TypeError") || td2_exc_is_instance(k, "ValueError")) {
                json_free(f);
                json_t *la = json_array();
                json_append(la, json_copy(valc));
                json_free(valc);
                json_t *lr = td2_logger(io, "warning",
                    "delegation.prefix_stagger_seconds=%r is not a number; the prompt-cache warm-up wait is off",
                    la, 0);
                if (td2_is_raise(lr)) return lr;
                json_free(lr);
                return json_number(0.0);
            }
            json_free(valc);
            return f;
        }
        double x = td2_num(f);
        json_free(f); json_free(valc);
        double clamped = x < 0 ? 0 : (x > 60 ? 60 : x);
        return json_number(clamped);
    }
    json_free(valc);
    return json_number(0.0);
}

/* =====================================================================
 * tools/delegate_tool_auto_resume.py — waiting_on_resume
 * ===================================================================== */

/* PoP: td2_waiting_on_resume @ tools/delegate_tool_auto_resume.py:waiting_on_resume */
static json_t *td2_waiting_on_resume(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *e = td2_get(in, "entry");
    json_t *ar = td2_get(e, "auto_resume");
    int r = td2_is_plain_dict(ar)
        || td2_truth(td2_get(e, "waiting_to_resume"))
        || td2_truth(td2_get(e, "unstarted_auto"));
    return json_bool(r);
}

/* =====================================================================
 * gateway/session_finite.py — finite_turn_required (ContextVar read)
 * ===================================================================== */

/* PoP: td2_finite_turn_required @ gateway/session_finite.py:finite_turn_required */
static json_t *td2_finite_turn_required(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *v = td2_get(in, "finite_turn");
    return v ? json_copy(v) : json_null();
}

/* =====================================================================
 * tools/process_registry_notifications.py — _process_accounting_lines
 * ===================================================================== */

/* dict.get(k, "") projection: absent -> "", present (incl null) -> str(). */
static char *td2_get_def_empty(const json_t *o, const char *k)
{
    json_t *v = td2_get(o, k);
    if (!v) return td2_xfmt("");
    return td2_pystr(v);
}

/* PoP: td2_process_accounting_lines @ tools/process_registry_notifications.py:_process_accounting_lines */
static json_t *td2_process_accounting_lines(td2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *r = td2_get(in, "r");
    json_t *out = json_array();
    /* handed_off_processes */
    json_t *ho = td2_get(r, "handed_off_processes");
    if (td2_is_arr(ho) && td2_truth(ho)) {
        for (size_t i = 0; i < json_len(ho); i++) {
            json_t *h = json_get(ho, i);
            char *sid = td2_pystr(td2_get(h, "session_id"));
            json_t *cmd = td2_get(h, "command");
            const char *cmds = td2_is_str(cmd) ? td2_str(cmd) : "";
            char *note = td2_get_def_empty(h, "note");
            char *line = td2_xfmt("Handed off to you: %s (%.120s) " TD2_EMDASH " %s. "
                "You own it now; its completion notice will arrive here.",
                sid ? sid : "None", cmds, note ? note : "");
            json_append(out, json_string(line ? line : ""));
            free(sid); free(note); free(line);
        }
    }
    /* orphaned_processes */
    json_t *orphans = td2_get(r, "orphaned_processes");
    if (td2_is_arr(orphans) && td2_truth(orphans)) {
        size_t n = json_len(orphans);
        char *joined = td2_xfmt("");
        for (size_t i = 0; i < n; i++) {
            json_t *o = json_get(orphans, i);
            char *sid = td2_pystr(td2_get(o, "session_id"));
            json_t *cmd = td2_get(o, "command");
            const char *cmds = td2_is_str(cmd) ? td2_str(cmd) : "";
            char *rt = td2_pystr(td2_get(o, "runtime_seconds"));
            char *part = td2_xfmt("%s%s `%.100s` (%ss)", i ? "; " : "", sid ? sid : "None", cmds, rt ? rt : "None");
            char *nj = td2_xfmt("%s%s", joined ? joined : "", part ? part : "");
            free(joined); free(part); free(sid); free(rt);
            joined = nj;
        }
        char *line = td2_xfmt("Child left %zu background process(es) running that were TERMINATED with it "
            "(subagent process notices never reach you): %s. Re-launch in this session anything you still need.",
            n, joined ? joined : "");
        json_append(out, json_string(line ? line : ""));
        free(joined); free(line);
    }
    /* unread_completions */
    json_t *uc = td2_get(r, "unread_completions");
    if (td2_is_arr(uc) && td2_truth(uc)) {
        for (size_t i = 0; i < json_len(uc); i++) {
            json_t *u = json_get(uc, i);
            char *sid = td2_pystr(td2_get(u, "session_id"));
            json_t *cmd = td2_get(u, "command");
            const char *cmds = td2_is_str(cmd) ? td2_str(cmd) : "";
            char *ec = td2_pystr(td2_get(u, "exit_code"));
            char *tail = td2_get_def_empty(u, "output_tail");
            char *line = td2_xfmt("Child's process %s `%.100s` finished (exit code %s) but the child never read "
                "its result; output tail:\n%s", sid ? sid : "None", cmds, ec ? ec : "None", tail ? tail : "");
            json_append(out, json_string(line ? line : ""));
            free(sid); free(ec); free(tail); free(line);
        }
    }
    return out;
}