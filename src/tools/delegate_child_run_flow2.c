/* delegate_child_run_flow2.c — FLOW port of the UNCLAIMED remainder of
 * tools/delegate_tool_child_run.py (LIVE authority in the quarry).
 *
 * tool_dispatch_flow2.c already claimed 12 defs (_num, _str_or_none,
 * _tier_in_effect, _route_report, _fabricated_entry, _append_missed_steer,
 * _close_child, _with_children_lock, _attach_child, _detach_child,
 * _signal_child_stop, _child_activity_fingerprint) and delegate_tool.c claimed
 * _dump_subagent_timeout_diagnostic. This port takes 17 REMAINING unclaimed
 * top-level functions:
 *
 *   _reserve_child_transcript, _format_thread_stack, _diag_section, _diag_sizes,
 *   _budget_warning_text, _warn_child_budget, _child_last_event_age,
 *   _register_child, _create_isolated_worktree, _lease_child_credential,
 *   _merge_late_steer, _validate_child_output_schema, _build_tool_trace,
 *   _build_result_entry, _is_image_url, _build_child_goal_message, _checkpointing
 *
 * The _Heartbeat / _ChildRun class machinery and _diag_threads /
 * _defer_close_after_timeout (live thread-frame / Timer I/O) are out of scope.
 *
 * Claimed siblings are SEAMED (never re-ported): _str_or_none, _num,
 * _route_report (universal-seam value calls) and _quiet (a claimed CM: the
 * factory call is the "quiet" seam, the except-Exception -> logger.debug +
 * suppress exit behaviour is transcribed per call site, BaseException passes).
 * Ported siblings compose for REAL (_diag_section inside _diag_sizes,
 * _budget_warning_text inside _warn_child_budget, _is_image_url inside
 * _build_child_goal_message, _build_tool_trace inside _build_result_entry).
 * Foreign unclaimed leaves are seamed too: get_session_env,
 * _capture_gateway_steer_authority, _register_subagent, _get_worktree_isolation,
 * local_backend_active, get_session_cwd, _resolve_workspace_hint,
 * create_subagent_worktree, child_profile_scope, delegated_child_context,
 * credential_pool_entry_serves_endpoint, _close_subagent_steering,
 * validate_output, build_retry_message, append_output_contract,
 * flatten_message_text, _summarize_tool_arguments, _stringify_tool_content,
 * _looks_like_error_output, decide_image_input_mode, build_native_content_parts,
 * load_config_readonly, NewSession, traceback.format_stack, os.path.isfile,
 * time.time and the child/pool method doubles.
 *
 * Decision sequences only: Python truthiness, or/and short-circuits, is None /
 * is True / is False identity vs truthy tests, kwarg/operand evaluation order
 * (the OUTER attribute chain resolves BEFORE an argument seam fires), dict
 * insertion order, and except-Exception / except-BaseException / except-
 * (TypeError,ValueError) catch scopes are transcribed per call site. A scripted
 * BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
 * propagates past every except Exception guard exactly as live (FULL builtin
 * exception-parent table below, audited against the py3.14 gate interpreter).
 *
 * Value model (shared with tests/oracle_delegate_child_run_flow2.py):
 *   - attribute doubles {"__attr__": {...}, "__id__": "c0"?, "__class__": tag?}
 *     <-> SimpleNamespace; the state window keeps the wrapper form and is
 *     mutated in place. __class__ "RuntimeSessionStore" gives isinstance identity.
 *   - callables project to {"__callable__": "<seam>"}; a callable marker may
 *     carry a top-level "_flush" key (_checkpointing's inner._flush).
 *   - raise specs {"__raise__": "<ClassName>", "message": str(e)}.
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - fixture text is ASCII; live em-dash (U+2014) literals are hex-escaped.
 *   - _diag_sizes tools_schema carries plain JSON values (str/int/bool/None/
 *     dict/list) only — no floats (json.dumps integral-float "1.0" vs int "1")
 *     and no non-serializable objects (default=str would emit a nondeterministic
 *     repr address). json.dumps separators ", " / ": " are transcribed.
 *   - _register_child child carries scalar attrs only (the entry's "agent" field
 *     embeds the unwrapped double); _delegate_depth avoids integral floats in the
 *     isinstance-int position (2.0 reads as int in libjson).
 *   - int()/float() string coercion covers ASCII-whitespace-padded signed digits
 *     (int: no underscores/exponent; float: exponent/inf/nan); raising forms
 *     (null/list/dict -> TypeError, non-numeric str -> ValueError) are in domain.
 *   - round(x,2)/round(x,6) use snprintf("%.*f")+strtod (probe-verified equal to
 *     Python round on a 25k corpus); :.0f / :g use "%.0f" / "%g" (probe-verified).
 *   - 2-tuple unpack scripts are arrays (or raising forms); str unpack iterates
 *     chars. _build_result_entry _stale_stop_seconds is int/float (no bool/str).
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified "tools/delegate_child_run_flow2.c".
 * Every symbol is static dcr2_*.
 *
 * Gate: bash tests/run_one_oracle.sh delegate_child_run_flow2 \
 *           oracle_delegate_child_run_flow2.py "" "lib/libjson/json.o"
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
/* U+2014 em dash (budget warning / result-entry / goal-message f-strings). */
#define DCR2_EMDASH "\xe2\x80\x94"

/* ── universal seam ── */
typedef struct dcr2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} dcr2_io_t;

/* ── truthiness / accessors ── */
static int dcr2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int dcr2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int dcr2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int dcr2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int dcr2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int dcr2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int dcr2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *dcr2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *dcr2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double dcr2_num(const json_t *v)
{
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* isinstance(x, dict): a PLAIN object, never an __attr__ double. */
static int dcr2_is_plain_dict(const json_t *v)
{
    return dcr2_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

/* isinstance(x, (int, float)): a number OR a bool (bool is an int subclass). */
static int dcr2_is_number_or_bool(const json_t *v)
{
    return dcr2_is_num(v) || dcr2_is_bool(v);
}

/* isinstance(x, int): number OR bool; integral floats read as int in libjson
 * (fixture domain avoids integral floats in int-check positions). */
static int dcr2_is_int(const json_t *v)
{
    if (dcr2_is_bool(v)) return 1;
    if (!dcr2_is_num(v)) return 0;
    double d = v->num_val;
    return isfinite(d) && d == (double)(long long)d;
}

static const char *dcr2_typename(const json_t *v)
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
            const char *tag = dcr2_str(dcr2_get(v, "__class__"));
            if (tag) return tag;
            if (json_obj_get(v, "__attr__")) return "types.SimpleNamespace";
            if (json_obj_get(v, "__callable__")) return "function";
            return "dict";
        }
        default: return "object";
    }
}

/* ── attribute doubles ── */
static json_t *dcr2_dattr(const json_t *d, const char *name)
{
    return dcr2_get(dcr2_get(d, "__attr__"), name);
}

/* getattr(d, name, default): a wrapper reads __attr__.name; None / non-wrapper
 * yields the default (getattr never raises when a default is given). */
static json_t *dcr2_getattr(const json_t *d, const char *name, json_t *defval)
{
    json_t *inner = dcr2_get(d, "__attr__");
    if (inner) {
        json_t *v = json_obj_get(inner, name);
        return v ? v : defval;
    }
    return defval;
}

/* hasattr(d, name): a wrapper carries the key in __attr__. */
static int dcr2_hasattr(const json_t *d, const char *name)
{
    json_t *inner = dcr2_get(d, "__attr__");
    return inner && json_obj_get(inner, name) != NULL;
}

static void dcr2_dset(const json_t *d, const char *name, json_t *val)
{
    json_t *inner = dcr2_get(d, "__attr__");
    if (inner && inner->type == JSON_OBJECT) json_set(inner, name, val);
    else json_free(val);
}

/* ── raise specs ── */
static int dcr2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *dcr2_kind(const json_t *r)
{
    const char *k = dcr2_str(dcr2_get(r, "__raise__"));
    return k ? k : "";
}

static const char *dcr2_raise_msg(const json_t *r)
{
    const char *m = dcr2_str(dcr2_get(r, "message"));
    return m ? m : "";
}

static json_t *dcr2_mkraise(const char *kind, const char *msg)
{
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}

/* ── FULL builtin exception-parent hierarchy (py3.14, audited against the gate
 *    interpreter's builtins). The BaseException trio (+ asyncio.CancelledError)
 *    parent to BaseException ONLY, so they pass through every except Exception. ── */
typedef struct { const char *name; const char *parent; } dcr2_exc_row;
static const dcr2_exc_row DCR2_EXC_PARENTS[] = {
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

static const char *dcr2_exc_parent(const char *name)
{
    for (size_t i = 0; DCR2_EXC_PARENTS[i].name; i++)
        if (strcmp(DCR2_EXC_PARENTS[i].name, name) == 0) return DCR2_EXC_PARENTS[i].parent;
    return "Exception";
}

static int dcr2_exc_is_instance(const char *kind, const char *base)
{
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, base) == 0) return 1;
        const char *p = dcr2_exc_parent(cur);
        if (!p || strcmp(cur, p) == 0) break;
        snprintf(cur, sizeof(cur), "%s", p);
        if (strcmp(cur, "BaseException") == 0) return strcmp(base, "BaseException") == 0;
    }
    return 0;
}

static int dcr2_exc_is_exception(const char *kind)
{
    return dcr2_exc_is_instance(kind, "Exception");
}

/* ── printf-style malloc'd format ── */
static char *dcr2_xfmt(const char *fmt, ...)
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
static json_t *dcr2_call(dcr2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = dcr2_xfmt("no seam installed for %s", name);
        r = dcr2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *dcr2_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides. */
static void dcr2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void dcr2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void dcr2_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void dcr2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* ── norm_value projection (mirror oracle norm_value): unwrap __attr__ doubles,
 *    keep callable markers, recurse arrays/plain dicts, normalize integral
 *    floats. Used to embed an object as a dict VALUE that rides norm_args. ── */
static json_t *dcr2_norm_value(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_OBJECT) {
        json_t *inner = json_obj_get(v, "__attr__");
        if (inner && inner->type == JSON_OBJECT) {
            json_t *o = json_object();
            for (size_t i = 0; i < inner->c.count; i++) {
                const char *k = inner->c.keys[i];
                if (strncmp(k, "__td2", 5) == 0) continue;
                json_set(o, k, dcr2_norm_value(inner->c.items[i]));
            }
            return o;
        }
        if (json_obj_get(v, "__callable__")) return json_copy(v);
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++)
            json_set(o, v->c.keys[i], dcr2_norm_value(v->c.items[i]));
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, dcr2_norm_value(v->c.items[i]));
        return a;
    }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18)
            return json_number((double)(long long)d);
        return json_copy(v);
    }
    return json_copy(v);
}

/* ── string helpers (ASCII domain) ── */
static json_t *dcr2_strn(const char *s, size_t n)
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
static char *dcr2_float_repr(double d)
{
    char buf[64];
    for (int p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*g", p, d);
        char *end = NULL;
        double back = strtod(buf, &end);
        if (end && *end == '\0' && back == d) return dcr2_xfmt("%s", buf);
    }
    snprintf(buf, sizeof(buf), "%.17g", d);
    return dcr2_xfmt("%s", buf);
}

/* Python str() projection (domain: str / number / bool / None). */
static char *dcr2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return dcr2_xfmt("None");
    if (v->type == JSON_BOOL) return dcr2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return dcr2_xfmt("%s", v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18)
            return dcr2_xfmt("%.0f", d);
        return dcr2_float_repr(d);
    }
    return dcr2_xfmt("");
}

/* Python s[:n] slice (n >= 0; past-the-end yields the whole string). */
static json_t *dcr2_slice(const json_t *s, size_t n)
{
    const char *p = dcr2_str(s);
    if (!p) return json_string("");
    size_t len = strlen(p);
    if (n >= len) return json_string(p);
    return dcr2_strn(p, n);
}

/* count '%' chars (live log_message.count("%")). */
static int dcr2_count_pct(const char *s)
{
    int n = 0;
    for (const char *p = s; p && *p; p++) if (*p == '%') n++;
    return n;
}

/* ASCII whitespace set shared by str.strip / rstrip (the 6 common ws plus the
 * 0x1c-0x1f file separators Python's White_Space includes). */
static int dcr2_is_strip_ws(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}

/* str.strip() both ends (ASCII domain). Returns malloc'd. */
static char *dcr2_strip(const char *s)
{
    if (!s) return dcr2_xfmt("");
    const char *a = s;
    while (*a && dcr2_is_strip_ws((unsigned char)*a)) a++;
    const char *b = s + strlen(s);
    while (b > a && dcr2_is_strip_ws((unsigned char)b[-1])) b--;
    return dcr2_xfmt("%.*s", (int)(b - a), a);
}

/* str.rstrip() (ASCII domain). Returns malloc'd. */
static char *dcr2_rstrip(const char *s)
{
    if (!s) return dcr2_xfmt("");
    const char *b = s + strlen(s);
    while (b > s && dcr2_is_strip_ws((unsigned char)b[-1])) b--;
    return dcr2_xfmt("%.*s", (int)(b - s), s);
}

/* Python round(x, nd) via correctly-rounded decimal formatting (probe-verified
 * equal to live round on a 25k corpus). Non-finite passes through. */
static json_t *dcr2_round_nd(double x, int nd)
{
    if (!isfinite(x)) return json_number(x);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", nd, x);
    return json_number(strtod(buf, NULL));
}

/* f"{x:.0f}" / f"{x:g}" (probe-verified equal to live on a 25k corpus). */
static char *dcr2_fmt_f0(double x)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.0f", x);
    return dcr2_xfmt("%s", buf);
}
static char *dcr2_fmt_g(double x)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", x);
    return dcr2_xfmt("%s", buf);
}

/* f"{v:.0f}" / f"{v:g}" on a non-number: str -> ValueError "Unknown format code
 * '<c>' for object of type 'str'"; other -> TypeError "unsupported format string
 * passed to <type>.__format__" (probe-verified py3.14 messages). */
static json_t *dcr2_fmt_code_raise(const json_t *v, char code)
{
    if (dcr2_is_str(v)) {
        char *m = dcr2_xfmt("Unknown format code '%c' for object of type 'str'", code);
        json_t *r = dcr2_mkraise("ValueError", m ? m : "format"); free(m); return r;
    }
    char *m = dcr2_xfmt("unsupported format string passed to %s.__format__", dcr2_typename(v));
    json_t *r = dcr2_mkraise("TypeError", m ? m : "format"); free(m); return r;
}

/* case-insensitive ASCII prefix test. */
static int dcr2_iprefix(const char *s, const char *pref)
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
static int dcr2_float_grammar(const char *s)
{
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    if (dcr2_iprefix(p, "inf")) {
        p += 3;
        if (dcr2_iprefix(p, "inity")) p += 5;
        return *p == '\0';
    }
    if (dcr2_iprefix(p, "nan")) { p += 3; return *p == '\0'; }
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
static json_t *dcr2_py_float(const json_t *v)
{
    if (dcr2_is_num(v)) return json_number(v->num_val);
    if (dcr2_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (dcr2_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        const char *a = s;
        while (*a && dcr2_is_strip_ws((unsigned char)*a)) a++;
        const char *b = s + strlen(s);
        while (b > a && dcr2_is_strip_ws((unsigned char)b[-1])) b--;
        size_t n = (size_t)(b - a);
        char *clean = (char *)malloc(n + 1);
        if (!clean) return dcr2_mkraise("ValueError", "could not convert string to float");
        memcpy(clean, a, n); clean[n] = '\0';
        if (n == 0 || !dcr2_float_grammar(clean)) {
            char *m = dcr2_xfmt("could not convert string to float: '%s'", clean);
            json_t *r = dcr2_mkraise("ValueError", m ? m : "could not convert string to float");
            free(m); free(clean);
            return r;
        }
        double d = strtod(clean, NULL);
        free(clean);
        return json_number(d);
    }
    char *m = dcr2_xfmt("float() argument must be a string or a real number, not '%s'", dcr2_typename(v));
    json_t *r = dcr2_mkraise("TypeError", m ? m : "float() argument");
    free(m);
    return r;
}

/* Python int(v): truncation toward zero for numbers/bools; ASCII-ws-padded
 * signed decimal digits for strings (no underscores/exponent in domain). */
static json_t *dcr2_py_int(const json_t *v)
{
    if (dcr2_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (dcr2_is_num(v)) return json_number((double)(long long)v->num_val);
    if (dcr2_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        /* int() strips the 6 common ASCII ws (NOT 0x1c-0x1f). */
        const char *a = s;
        while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\v' || *a == '\f') a++;
        const char *b = s + strlen(s);
        while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' || b[-1] == '\r' || b[-1] == '\v' || b[-1] == '\f')) b--;
        size_t n = (size_t)(b - a);
        char *clean = (char *)malloc(n + 1);
        if (!clean) return dcr2_mkraise("ValueError", "invalid literal for int() with base 10");
        memcpy(clean, a, n); clean[n] = '\0';
        const char *p = clean;
        if (*p == '+' || *p == '-') p++;
        int digits = 0;
        while (isdigit((unsigned char)*p)) { p++; digits = 1; }
        if (!digits || *p != '\0') {
            char *m = dcr2_xfmt("invalid literal for int() with base 10: '%s'", clean);
            json_t *r = dcr2_mkraise("ValueError", m ? m : "invalid literal for int()");
            free(m); free(clean);
            return r;
        }
        long long ll = strtoll(clean, NULL, 10);
        free(clean);
        return json_number((double)ll);
    }
    char *m = dcr2_xfmt("int() argument must be a string, a bytes-like object or a real number, not '%s'", dcr2_typename(v));
    json_t *r = dcr2_mkraise("TypeError", m ? m : "int() argument");
    free(m);
    return r;
}

/* AttributeError raise spec for a missing attribute on a carrier. */
static json_t *dcr2_attr_raise(const json_t *obj, const char *name)
{
    char *m = dcr2_xfmt("'%s' object has no attribute '%s'", dcr2_typename(obj), name);
    json_t *r = dcr2_mkraise("AttributeError", m ? m : "attribute error");
    free(m);
    return r;
}

/* TypeError raise spec for calling a non-callable. */
static json_t *dcr2_not_callable_raise(const json_t *v)
{
    char *m = dcr2_xfmt("'%s' object is not callable", dcr2_typename(v));
    json_t *r = dcr2_mkraise("TypeError", m ? m : "not callable");
    free(m);
    return r;
}

/* a callable double {"__callable__": seam}. */
static int dcr2_is_callable(const json_t *v)
{
    return dcr2_is_obj(v) && json_obj_get(v, "__callable__") != NULL;
}

/* ── logger seam (args recorded VERBATIM; %s/%r apply only at emission) ── */
static json_t *dcr2_logger(dcr2_io_t *io, const char *level, const char *msg, json_t *args_arr, int exc_info)
{
    json_t *a = dcr2_args();
    dcr2_arg_s(a, "msg", msg);
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) dcr2_arg_b(a, "exc_info", 1);
    char *nm = dcr2_xfmt("logger.%s", level);
    json_t *r = dcr2_call(io, nm ? nm : "logger.info", a);
    free(nm);
    return r;
}

/* _quiet factory seam (claimed CM): fires "quiet" with the factory args.
 * Returns a raise spec to propagate, or NULL on success. */
static json_t *dcr2_quiet_factory(dcr2_io_t *io, const char *msg, const json_t *log_args, int exc_info)
{
    json_t *a = dcr2_args();
    dcr2_arg_s(a, "log_message", msg);
    if (dcr2_is_arr(log_args) && json_len(log_args) > 0) json_set(a, "log_args", json_copy(log_args));
    if (exc_info) dcr2_arg_b(a, "exc_info", 1);
    json_t *r = dcr2_call(io, "quiet", a);
    if (dcr2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* _quiet exit: swallow an Exception raise; a non-None log_message is logged at
 * debug with the exception filling a trailing unsatisfied %s. log_args BORROWED.
 * Returns NULL when swallowed, the logger.debug raise if that call raises, or
 * the BaseException raise (NOT swallowed) for the caller to propagate. */
static json_t *dcr2_quiet_exit(dcr2_io_t *io, const char *msg, const json_t *log_args, int exc_info, json_t *r)
{
    if (!dcr2_exc_is_exception(dcr2_kind(r))) return r;  /* BaseException passes */
    if (!msg) { json_free(r); return NULL; }
    int pct = dcr2_count_pct(msg);
    size_t nargs = dcr2_is_arr(log_args) ? json_len(log_args) : 0;
    json_t *la = json_array();
    for (size_t i = 0; i < nargs; i++) json_append(la, json_copy(json_get(log_args, i)));
    if (pct > (int)nargs) json_append(la, json_string(dcr2_raise_msg(r)));
    json_free(r);
    return dcr2_logger(io, "debug", msg, la, exc_info);
}

/* a silent CM factory seam (child_profile_scope / delegated_child_context):
 * enter/exit are no-ops in the single-threaded harness; only the factory call
 * rides the seam. Returns a raise spec to propagate, or NULL. */
static json_t *dcr2_cm_factory(dcr2_io_t *io, const char *name, json_t *args)
{
    json_t *r = dcr2_call(io, name, args);
    if (dcr2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── claimed-sibling value seams ── */
static json_t *dcr2_str_or_none_seam(dcr2_io_t *io, const json_t *v)
{
    json_t *a = dcr2_args();
    dcr2_arg_j(a, "value", v);
    return dcr2_call(io, "_str_or_none", a);
}
static json_t *dcr2_num_seam(dcr2_io_t *io, const json_t *v)
{
    json_t *a = dcr2_args();
    dcr2_arg_j(a, "value", v);
    return dcr2_call(io, "_num", a);
}
static json_t *dcr2_route_report_seam(dcr2_io_t *io, const json_t *child)
{
    json_t *a = dcr2_args();
    dcr2_arg_j(a, "child", child);
    return dcr2_call(io, "_route_report", a);
}

/* ── list(v) coercion: array copy / str chars / dict keys / else TypeError ── */
static json_t *dcr2_list_coerce(const json_t *v)
{
    if (dcr2_is_arr(v)) return json_copy(v);
    if (dcr2_is_str(v)) {
        json_t *a = json_array();
        const char *s = v->str_val ? v->str_val : "";
        for (const char *p = s; *p; p++) json_append(a, dcr2_strn(p, 1));
        return a;
    }
    if (dcr2_is_obj(v)) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, json_string(v->c.keys[i]));
        return a;
    }
    char *m = dcr2_xfmt("'%s' object is not iterable", dcr2_typename(v));
    json_t *r = dcr2_mkraise("TypeError", m ? m : "not iterable");
    free(m);
    return r;
}

/* ── 2-tuple unpack (arrays / str chars; raising forms). On success the out
 *    params own copies; returns NULL. On failure returns a raise spec. ── */
static json_t *dcr2_unpack2(const json_t *v, json_t **a, json_t **b)
{
    if (dcr2_is_arr(v)) {
        size_t n = json_len(v);
        if (n < 2) {
            char *m = dcr2_xfmt("not enough values to unpack (expected 2, got %zu)", n);
            json_t *r = dcr2_mkraise("ValueError", m ? m : "unpack"); free(m); return r;
        }
        if (n > 2) {
            char *m = dcr2_xfmt("too many values to unpack (expected 2, got %zu)", n);
            json_t *r = dcr2_mkraise("ValueError", m ? m : "unpack"); free(m); return r;
        }
        *a = json_copy(json_get(v, 0));
        *b = json_copy(json_get(v, 1));
        return NULL;
    }
    if (dcr2_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s);
        if (n < 2) {
            char *m = dcr2_xfmt("not enough values to unpack (expected 2, got %zu)", n);
            json_t *r = dcr2_mkraise("ValueError", m ? m : "unpack"); free(m); return r;
        }
        if (n > 2) {
            /* py3.14 STRING overflow omits "got N". */
            json_t *r = dcr2_mkraise("ValueError", "too many values to unpack (expected 2)");
            return r;
        }
        *a = dcr2_strn(s, 1);
        *b = dcr2_strn(s + 1, 1);
        return NULL;
    }
    char *m = dcr2_xfmt("cannot unpack non-iterable %s object", dcr2_typename(v));
    json_t *r = dcr2_mkraise("TypeError", m ? m : "unpack"); free(m);
    return r;
}

/* ── ", ".join / "\n".join over str elements (TypeError on non-str). ── */
static json_t *dcr2_join(const json_t *arr, const char *sep)
{
    if (!dcr2_is_arr(arr)) return dcr2_mkraise("TypeError", "can only join an iterable");
    size_t n = json_len(arr);
    size_t cap = 64;
    char *buf = (char *)malloc(cap);
    if (!buf) return json_string("");
    size_t len = 0;
    buf[0] = '\0';
    size_t seplen = strlen(sep);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(arr, i);
        if (!dcr2_is_str(e)) {
            char *m = dcr2_xfmt("sequence item %zu: expected str instance, %s found", i, dcr2_typename(e));
            json_t *r = dcr2_mkraise("TypeError", m ? m : "join"); free(m); free(buf); return r;
        }
        const char *s = e->str_val ? e->str_val : "";
        size_t sl = strlen(s);
        size_t need = len + sl + (i ? seplen : 0) + 1;
        if (need > cap) { while (cap < need) cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return json_string(""); } buf = nb; }
        if (i) { memcpy(buf + len, sep, seplen); len += seplen; }
        memcpy(buf + len, s, sl); len += sl; buf[len] = '\0';
    }
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

/* ── Python json.dumps(v, default=str) for the plain-JSON domain (separators
 *    ", " / ": ", ensure_ascii). Integral numbers render as ints; non-integral
 *    as float repr; non-JSON values fall back to str() (default=str). ── */
static void dcr2_dumps_buf(const json_t *v, char **buf, size_t *len, size_t *cap);

static void dcr2_buf_append(char **buf, size_t *len, size_t *cap, const char *s)
{
    size_t sl = strlen(s);
    size_t need = *len + sl + 1;
    if (need > *cap) { while (*cap < need) *cap *= 2; char *nb = (char *)realloc(*buf, *cap); if (!nb) return; *buf = nb; }
    memcpy(*buf + *len, s, sl); *len += sl; (*buf)[*len] = '\0';
}

/* JSON string escaping matching Python json.dumps ensure_ascii (ASCII domain). */
static void dcr2_dumps_str(const char *s, char **buf, size_t *len, size_t *cap)
{
    dcr2_buf_append(buf, len, cap, "\"");
    for (const char *p = s; p && *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"': dcr2_buf_append(buf, len, cap, "\\\""); break;
            case '\\': dcr2_buf_append(buf, len, cap, "\\\\"); break;
            case '\n': dcr2_buf_append(buf, len, cap, "\\n"); break;
            case '\r': dcr2_buf_append(buf, len, cap, "\\r"); break;
            case '\t': dcr2_buf_append(buf, len, cap, "\\t"); break;
            case '\b': dcr2_buf_append(buf, len, cap, "\\b"); break;
            case '\f': dcr2_buf_append(buf, len, cap, "\\f"); break;
            default:
                if (c < 0x20) {
                    char esc[8]; snprintf(esc, sizeof(esc), "\\u%04x", c);
                    dcr2_buf_append(buf, len, cap, esc);
                } else {
                    char ch[2] = { (char)c, '\0' };
                    dcr2_buf_append(buf, len, cap, ch);
                }
        }
    }
    dcr2_buf_append(buf, len, cap, "\"");
}

static void dcr2_dumps_buf(const json_t *v, char **buf, size_t *len, size_t *cap)
{
    if (!v || v->type == JSON_NULL) { dcr2_buf_append(buf, len, cap, "null"); return; }
    if (v->type == JSON_BOOL) { dcr2_buf_append(buf, len, cap, v->bool_val ? "true" : "false"); return; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18) {
            char *s = dcr2_xfmt("%.0f", d); dcr2_buf_append(buf, len, cap, s ? s : "0"); free(s);
        } else {
            char *s = dcr2_float_repr(d); dcr2_buf_append(buf, len, cap, s ? s : "0"); free(s);
        }
        return;
    }
    if (v->type == JSON_STRING) { dcr2_dumps_str(v->str_val ? v->str_val : "", buf, len, cap); return; }
    if (v->type == JSON_ARRAY) {
        dcr2_buf_append(buf, len, cap, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) dcr2_buf_append(buf, len, cap, ", ");
            dcr2_dumps_buf(v->c.items[i], buf, len, cap);
        }
        dcr2_buf_append(buf, len, cap, "]");
        return;
    }
    if (v->type == JSON_OBJECT) {
        /* an __attr__ double / callable marker is non-JSON -> default=str. */
        if (json_obj_get(v, "__attr__") || json_obj_get(v, "__callable__")) {
            char *s = dcr2_pystr(v); dcr2_dumps_str(s ? s : "", buf, len, cap); free(s);
            return;
        }
        dcr2_buf_append(buf, len, cap, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) dcr2_buf_append(buf, len, cap, ", ");
            dcr2_dumps_str(v->c.keys[i], buf, len, cap);
            dcr2_buf_append(buf, len, cap, ": ");
            dcr2_dumps_buf(v->c.items[i], buf, len, cap);
        }
        dcr2_buf_append(buf, len, cap, "}");
        return;
    }
}

static char *dcr2_json_dumps(const json_t *v)
{
    size_t cap = 64, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return dcr2_xfmt("");
    buf[0] = '\0';
    dcr2_dumps_buf(v, &buf, &len, &cap);
    return buf;
}

/* =====================================================================
 * _is_image_url
 * ===================================================================== */
/* PoP: dcr2_is_image_url @ tools/delegate_tool_child_run.py:_is_image_url */
static json_t *dcr2_is_image_url_fn(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *ref = dcr2_get(in, "ref");
    if (!dcr2_is_str(ref)) return dcr2_attr_raise(ref, "startswith");
    const char *s = ref->str_val ? ref->str_val : "";
    int hit = strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0
           || strncmp(s, "data:image/", 11) == 0;
    return json_bool(hit ? true : false);
}

/* value helper composing the ported _is_image_url (no seam). */
static int dcr2_is_image_url_v(const json_t *ref)
{
    if (!dcr2_is_str(ref)) return 0;
    const char *s = ref->str_val ? ref->str_val : "";
    return strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0
        || strncmp(s, "data:image/", 11) == 0;
}

/* =====================================================================
 * _budget_warning_text
 * ===================================================================== */
/* PoP: dcr2_budget_warning_text @ tools/delegate_tool_child_run.py:_budget_warning_text */
static json_t *dcr2_budget_warning_text_fn(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *idle = dcr2_get(in, "idle_seconds");
    json_t *ct = dcr2_get(in, "child_timeout");
    if (!dcr2_is_number_or_bool(idle)) return dcr2_fmt_code_raise(idle, 'f');
    if (!dcr2_is_number_or_bool(ct)) return dcr2_fmt_code_raise(ct, 'f');
    char *a = dcr2_fmt_f0(dcr2_num(idle));
    char *b = dcr2_fmt_f0(dcr2_num(ct));
    char *s = dcr2_xfmt("[delegation budget warning] No progress signal for %ss of your %ss "
        "inactivity window. Finish the current step and return your summary now " DCR2_EMDASH
        " the work is discarded if the window elapses.", a ? a : "", b ? b : "");
    free(a); free(b);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* value helper (composes for REAL inside _warn_child_budget). May raise. */
static json_t *dcr2_budget_warning_text_v(const json_t *idle, const json_t *ct)
{
    if (!dcr2_is_number_or_bool(idle)) return dcr2_fmt_code_raise(idle, 'f');
    if (!dcr2_is_number_or_bool(ct)) return dcr2_fmt_code_raise(ct, 'f');
    char *a = dcr2_fmt_f0(dcr2_num(idle));
    char *b = dcr2_fmt_f0(dcr2_num(ct));
    char *s = dcr2_xfmt("[delegation budget warning] No progress signal for %ss of your %ss "
        "inactivity window. Finish the current step and return your summary now " DCR2_EMDASH
        " the work is discarded if the window elapses.", a ? a : "", b ? b : "");
    free(a); free(b);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* =====================================================================
 * _warn_child_budget
 * ===================================================================== */
/* PoP: dcr2_warn_child_budget @ tools/delegate_tool_child_run.py:_warn_child_budget */
static json_t *dcr2_warn_child_budget(dcr2_io_t *io, const json_t *in, json_t *st)
{
    json_t *child = dcr2_get(st, "child");
    json_t *idle = dcr2_get(in, "idle_seconds");
    json_t *ct = dcr2_get(in, "child_timeout");
    json_t *steer = dcr2_getattr(child, "steer", NULL);
    if (!dcr2_is_callable(steer)) return json_null();
    /* try: steer(_budget_warning_text(idle, ct)) — argument evaluated first. */
    json_t *text = dcr2_budget_warning_text_v(idle, ct);
    json_t *body_raise = NULL;
    if (dcr2_is_raise(text)) {
        body_raise = text;
    } else {
        const char *seam = dcr2_str(dcr2_get(steer, "__callable__"));
        json_t *a = dcr2_args();
        dcr2_arg_j(a, "text", text);
        json_free(text);
        json_t *r = dcr2_call(io, seam ? seam : "child.steer", a);
        if (dcr2_is_raise(r)) body_raise = r; else json_free(r);
    }
    if (body_raise) {
        /* except Exception as exc: logger.debug("budget warning steer failed: %s", exc) */
        if (!dcr2_exc_is_exception(dcr2_kind(body_raise))) return body_raise;
        json_t *la = json_array();
        json_append(la, json_string(dcr2_raise_msg(body_raise)));
        json_free(body_raise);
        json_t *lr = dcr2_logger(io, "debug", "budget warning steer failed: %s", la, 0);
        if (dcr2_is_raise(lr)) return lr;
        json_free(lr);
    }
    return json_null();
}

/* =====================================================================
 * _format_thread_stack
 * ===================================================================== */
/* PoP: dcr2_format_thread_stack @ tools/delegate_tool_child_run.py:_format_thread_stack */
static json_t *dcr2_format_thread_stack(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *frame = dcr2_get(in, "frame");
    const char *indent = dcr2_str(dcr2_get(in, "indent"));
    if (!indent) indent = "";
    json_t *a = dcr2_args();
    dcr2_arg_j(a, "frame", frame);
    json_t *fs = dcr2_call(io, "traceback.format_stack", a);
    if (dcr2_is_raise(fs)) return fs;
    /* [f"{indent}{sub}" for frame_line in fs for sub in frame_line.rstrip().split("\n")] */
    if (!dcr2_is_arr(fs)) {
        char *m = dcr2_xfmt("'%s' object is not iterable", dcr2_typename(fs));
        json_t *r = dcr2_mkraise("TypeError", m ? m : "not iterable"); free(m); json_free(fs); return r;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(fs); i++) {
        json_t *fl = json_get(fs, i);
        if (!dcr2_is_str(fl)) {
            json_t *r = dcr2_attr_raise(fl, "rstrip");
            json_free(out); json_free(fs); return r;
        }
        char *rs = dcr2_rstrip(fl->str_val ? fl->str_val : "");
        /* split("\n") */
        const char *p = rs ? rs : "";
        const char *start = p;
        for (;;) {
            const char *nl = strchr(start, '\n');
            size_t n = nl ? (size_t)(nl - start) : strlen(start);
            char *line = dcr2_xfmt("%s%.*s", indent, (int)n, start);
            json_append(out, json_string(line ? line : ""));
            free(line);
            if (!nl) break;
            start = nl + 1;
        }
        free(rs);
    }
    json_free(fs);
    return out;
}

/* =====================================================================
 * _diag_section
 * ===================================================================== */
/* core: produce is an internal helper (no seam) OR a callable marker seam.
 * dcr2_diag_section_core wraps a produce callback returning list-or-raise. */
typedef json_t *(*dcr2_produce_fn)(dcr2_io_t *io, const json_t *ctx);

static json_t *dcr2_diag_section_core(dcr2_io_t *io, const char *label, dcr2_produce_fn produce, const json_t *ctx)
{
    json_t *prod = produce(io, ctx);
    if (dcr2_is_raise(prod)) {
        if (!dcr2_exc_is_exception(dcr2_kind(prod))) return prod;  /* BaseException passes */
        char *s = dcr2_xfmt("  %s%s>", label, dcr2_raise_msg(prod));
        json_free(prod);
        json_t *out = json_array();
        json_append(out, json_string(s ? s : ""));
        free(s);
        return out;
    }
    json_t *lst = dcr2_list_coerce(prod);
    json_free(prod);
    if (dcr2_is_raise(lst)) {
        if (!dcr2_exc_is_exception(dcr2_kind(lst))) return lst;
        char *s = dcr2_xfmt("  %s%s>", label, dcr2_raise_msg(lst));
        json_free(lst);
        json_t *out = json_array();
        json_append(out, json_string(s ? s : ""));
        free(s);
        return out;
    }
    return lst;
}

/* produce as a callable-marker seam (direct drive). */
static json_t *dcr2_diag_produce_seam(dcr2_io_t *io, const json_t *ctx)
{
    const json_t *produce = ctx;  /* the callable marker itself */
    if (!dcr2_is_callable(produce)) return dcr2_not_callable_raise(produce);
    const char *seam = dcr2_str(dcr2_get(produce, "__callable__"));
    return dcr2_call(io, seam ? seam : "diag.produce", dcr2_args());
}

/* PoP: dcr2_diag_section @ tools/delegate_tool_child_run.py:_diag_section */
static json_t *dcr2_diag_section(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *label = dcr2_str(dcr2_get(in, "label"));
    if (!label) label = "";
    json_t *produce = dcr2_get(in, "produce");
    return dcr2_diag_section_core(io, label, dcr2_diag_produce_seam, produce);
}

/* =====================================================================
 * _diag_sizes (composes _diag_section for REAL with internal closures)
 * ===================================================================== */
static json_t *dcr2_diag_prompt(dcr2_io_t *io, const json_t *child)
{
    (void)io;
    /* sys_prompt = getattr(ephemeral) or getattr(system_prompt) or "" */
    json_t *sp = dcr2_getattr(child, "ephemeral_system_prompt", NULL);
    if (!dcr2_truth(sp)) sp = dcr2_getattr(child, "system_prompt", NULL);
    json_t *fallback = NULL;
    if (!dcr2_truth(sp)) { fallback = json_string(""); sp = fallback; }  /* ... or "" */
    int is_str = dcr2_is_str(sp);
    const char *s = is_str ? (sp->str_val ? sp->str_val : "") : "";
    size_t slen = strlen(s);  /* ASCII: utf-8 bytes == chars */
    char *b1 = is_str ? dcr2_xfmt("  system_prompt_bytes: %zu", slen) : dcr2_xfmt("  system_prompt_bytes: n/a");
    char *b2 = is_str ? dcr2_xfmt("  system_prompt_chars: %zu", slen) : dcr2_xfmt("  system_prompt_chars: n/a");
    json_t *out = json_array();
    json_append(out, json_string(b1 ? b1 : ""));
    json_append(out, json_string(b2 ? b2 : ""));
    free(b1); free(b2);
    json_free(fallback);
    return out;
}

static json_t *dcr2_diag_tools(dcr2_io_t *io, const json_t *child)
{
    (void)io;
    json_t *ts = dcr2_getattr(child, "tools", NULL);
    if (dcr2_is_null(ts)) return json_array();
    /* len(tools_schema) */
    size_t count;
    if (dcr2_is_arr(ts) || dcr2_is_obj(ts)) count = ts->c.count;
    else if (dcr2_is_str(ts)) count = strlen(ts->str_val ? ts->str_val : "");
    else {
        char *m = dcr2_xfmt("object of type '%s' has no len()", dcr2_typename(ts));
        json_t *r = dcr2_mkraise("TypeError", m ? m : "no len"); free(m); return r;
    }
    char *dumps = dcr2_json_dumps(ts);
    size_t bytes = strlen(dumps ? dumps : "");  /* ASCII */
    free(dumps);
    char *l1 = dcr2_xfmt("  tool_schema_count: %zu", count);
    char *l2 = dcr2_xfmt("  tool_schema_bytes: %zu", bytes);
    json_t *out = json_array();
    json_append(out, json_string(l1 ? l1 : ""));
    json_append(out, json_string(l2 ? l2 : ""));
    free(l1); free(l2);
    return out;
}

/* PoP: dcr2_diag_sizes @ tools/delegate_tool_child_run.py:_diag_sizes */
static json_t *dcr2_diag_sizes(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *child = dcr2_get(st, "child");
    json_t *out = json_array();
    json_append(out, json_string("## Prompt / schema sizes"));
    json_t *p = dcr2_diag_section_core(io, "system_prompt: <error: ", dcr2_diag_prompt, child);
    if (dcr2_is_raise(p)) { json_free(out); return p; }
    for (size_t i = 0; i < json_len(p); i++) json_append(out, json_copy(json_get(p, i)));
    json_free(p);
    json_t *t = dcr2_diag_section_core(io, "tool_schema: <error: ", dcr2_diag_tools, child);
    if (dcr2_is_raise(t)) { json_free(out); return t; }
    for (size_t i = 0; i < json_len(t); i++) json_append(out, json_copy(json_get(t, i)));
    json_free(t);
    return out;
}

/* =====================================================================
 * _child_last_event_age
 * ===================================================================== */
/* PoP: dcr2_child_last_event_age @ tools/delegate_tool_child_run.py:_child_last_event_age */
static json_t *dcr2_child_last_event_age(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *child = dcr2_get(st, "child");
    /* try: ts = (child.get_activity_summary() or {}).get("last_activity_ts").
     * child.get_activity_summary is a DIRECT attribute read: missing ->
     * AttributeError, non-callable -> TypeError; both caught by except Exception
     * -> None (no seam fires). */
    if (!dcr2_hasattr(child, "get_activity_summary")) return json_null();
    json_t *gas = dcr2_dattr(child, "get_activity_summary");
    if (!dcr2_is_callable(gas)) return json_null();
    const char *gas_seam = dcr2_str(dcr2_get(gas, "__callable__"));
    json_t *summ = dcr2_call(io, gas_seam ? gas_seam : "child.get_activity_summary", dcr2_args());
    if (dcr2_is_raise(summ)) {
        if (dcr2_exc_is_exception(dcr2_kind(summ))) { json_free(summ); return json_null(); }
        return summ;
    }
    json_t *dict;
    if (!dcr2_truth(summ)) { json_free(summ); dict = json_object(); }
    else dict = summ;
    if (!dcr2_is_plain_dict(dict)) {
        /* .get on a truthy non-dict raises AttributeError (caught). */
        json_free(dict);
        return json_null();
    }
    /* Snapshot BEFORE freeing dict: dcr2_get borrows the node owned by dict,
     * so using ts after json_free(dict) is a UAF (heap-layout-dependent wrong
     * ret: 0 / null instead of the real age). */
    json_t *ts_borrowed = dcr2_get(dict, "last_activity_ts");
    json_t *ts = ts_borrowed ? json_copy(ts_borrowed) : NULL;
    json_free(dict);
    if (dcr2_is_null(ts)) { json_free(ts); return json_null(); }  /* ts is None */
    /* round(max(0.0, time.time() - float(ts)), 2): the LEFT operand time.time()
     * evaluates BEFORE float(ts) (binary operands left-to-right). */
    json_t *now = dcr2_call(io, "time.time", dcr2_args());
    if (dcr2_is_raise(now)) {
        if (dcr2_exc_is_exception(dcr2_kind(now))) { json_free(now); return json_null(); }
        return now;
    }
    json_t *ft = dcr2_py_float(ts);
    if (dcr2_is_raise(ft)) {
        json_free(now);
        if (dcr2_exc_is_exception(dcr2_kind(ft))) { json_free(ft); return json_null(); }
        return ft;
    }
    double diff = dcr2_num(now) - dcr2_num(ft);
    json_free(now); json_free(ft);
    double m = diff > 0.0 ? diff : 0.0;
    return dcr2_round_nd(m, 2);
}

/* =====================================================================
 * _merge_late_steer
 * ===================================================================== */
/* PoP: dcr2_merge_late_steer @ tools/delegate_tool_child_run.py:_merge_late_steer */
static json_t *dcr2_merge_late_steer(dcr2_io_t *io, const json_t *in, json_t *st)
{
    json_t *result = dcr2_get(st, "result");
    json_t *child = dcr2_get(st, "child");
    json_t *sid = dcr2_get(in, "subagent_id");
    json_t *late = NULL;
    if (dcr2_truth(sid)) {
        json_t *a = dcr2_args();
        dcr2_arg_j(a, "subagent_id", sid);
        dcr2_arg_j(a, "child", child);
        late = dcr2_call(io, "_close_subagent_steering", a);
        if (dcr2_is_raise(late)) return late;
    }
    if (!dcr2_truth(late)) { json_free(late); return json_null(); }
    json_t *existing = dcr2_get(result, "pending_steer");
    json_t *val;
    if (dcr2_is_str(existing) && dcr2_truth(existing)) {
        char *s = dcr2_xfmt("%s\n%s", existing->str_val ? existing->str_val : "", dcr2_str(late) ? dcr2_str(late) : "");
        val = json_string(s ? s : "");
        free(s);
    } else {
        val = json_copy(late);
    }
    json_set(result, "pending_steer", val);
    json_free(late);
    return json_null();
}

/* =====================================================================
 * _reserve_child_transcript
 * ===================================================================== */
/* PoP: dcr2_reserve_child_transcript @ tools/delegate_tool_child_run.py:_reserve_child_transcript */
static json_t *dcr2_reserve_child_transcript(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *child = dcr2_get(st, "child");
    /* db = child._session_db (direct read) */
    if (!dcr2_hasattr(child, "_session_db")) return dcr2_attr_raise(child, "_session_db");
    json_t *db = dcr2_dattr(child, "_session_db");
    /* if db is None or child._persist_disabled or isinstance(db, RuntimeSessionStore): return */
    if (dcr2_is_null(db)) return json_null();
    if (!dcr2_hasattr(child, "_persist_disabled")) return dcr2_attr_raise(child, "_persist_disabled");
    json_t *pd = dcr2_dattr(child, "_persist_disabled");
    if (dcr2_truth(pd)) return json_null();
    const char *dbclass = dcr2_str(dcr2_get(db, "__class__"));
    if (dbclass && strcmp(dbclass, "RuntimeSessionStore") == 0) return json_null();
    /* try: ... except BaseException: child.close(); raise */
    json_t *body_raise = NULL;
    /* parent_sid = child._parent_session_id (direct read) */
    if (!dcr2_hasattr(child, "_parent_session_id")) {
        body_raise = dcr2_attr_raise(child, "_parent_session_id");
        goto handle;
    }
    json_t *parent_sid = dcr2_dattr(child, "_parent_session_id");
    /* resolve OUTER attribute chain child.persistence.sessions.create BEFORE
     * the NewSession argument seam fires. */
    if (!dcr2_hasattr(child, "persistence")) { body_raise = dcr2_attr_raise(child, "persistence"); goto handle; }
    json_t *persistence = dcr2_dattr(child, "persistence");
    if (!dcr2_hasattr(persistence, "sessions")) { body_raise = dcr2_attr_raise(persistence, "sessions"); goto handle; }
    json_t *sessions = dcr2_dattr(persistence, "sessions");
    if (!dcr2_hasattr(sessions, "create")) { body_raise = dcr2_attr_raise(sessions, "create"); goto handle; }
    json_t *create = dcr2_dattr(sessions, "create");
    /* NewSession(session_id=child.session_id, source="subagent",
     *            parent_session_id=parent_sid, model_config={...} if parent_sid else None) */
    if (!dcr2_hasattr(child, "session_id")) { body_raise = dcr2_attr_raise(child, "session_id"); goto handle; }
    json_t *session_id = dcr2_dattr(child, "session_id");
    {
        json_t *na = dcr2_args();
        dcr2_arg_j(na, "session_id", session_id);
        dcr2_arg_s(na, "source", "subagent");
        dcr2_arg_j(na, "parent_session_id", parent_sid);
        if (dcr2_truth(parent_sid)) {
            json_t *mc = json_object();
            json_set(mc, "_delegate_from", json_copy(parent_sid));
            json_set(na, "model_config", mc);
        }
        json_t *ns = dcr2_call(io, "NewSession", na);
        if (dcr2_is_raise(ns)) { body_raise = ns; goto handle; }
        /* child.persistence.sessions.create(NewSession(...)) */
        if (!dcr2_is_callable(create)) {
            json_free(ns);
            body_raise = dcr2_not_callable_raise(create);
            goto handle;
        }
        const char *seam = dcr2_str(dcr2_get(create, "__callable__"));
        json_t *ca = dcr2_args();
        dcr2_arg_j(ca, "new_session", ns);
        json_free(ns);
        json_t *cr = dcr2_call(io, seam ? seam : "child.persistence.sessions.create", ca);
        if (dcr2_is_raise(cr)) { body_raise = cr; goto handle; }
        json_free(cr);
    }
    /* child._session_db_reserved = True */
    dcr2_dset(child, "_session_db_reserved", json_bool(true));
    return json_null();

handle:
    /* except BaseException: child.close(); raise */
    {
        json_t *close = dcr2_getattr(child, "close", NULL);
        json_t *close_raise = NULL;
        if (dcr2_is_callable(close)) {
            const char *seam = dcr2_str(dcr2_get(close, "__callable__"));
            json_t *r = dcr2_call(io, seam ? seam : "child.close", dcr2_args());
            if (dcr2_is_raise(r)) close_raise = r; else json_free(r);
        } else if (dcr2_is_null(close)) {
            close_raise = dcr2_attr_raise(child, "close");
        } else {
            close_raise = dcr2_not_callable_raise(close);
        }
        if (close_raise) { json_free(body_raise); return close_raise; }
        return body_raise;
    }
}

/* =====================================================================
 * _register_child
 * ===================================================================== */
/* PoP: dcr2_register_child @ tools/delegate_tool_child_run.py:_register_child */
static json_t *dcr2_register_child(dcr2_io_t *io, const json_t *in, json_t *st)
{
    json_t *child = dcr2_get(st, "child");
    json_t *parent = dcr2_get(st, "parent_agent");
    json_t *goal = dcr2_get(in, "goal");
    json_t *owner_sid = dcr2_get(in, "owner_session_id");
    json_t *owner_transport = dcr2_get(in, "owner_transport");
    json_t *owner_record = dcr2_get(in, "owner_session_record");
    /* keep owned copies of the mutable owner fields */
    json_t *osid = owner_sid ? json_copy(owner_sid) : json_null();
    json_t *otrans = owner_transport ? json_copy(owner_transport) : json_null();
    json_t *orec = owner_record ? json_copy(owner_record) : json_null();

    json_t *sid = dcr2_getattr(child, "_subagent_id", NULL);
    if (!dcr2_is_str(sid) || !dcr2_truth(sid)) {
        json_free(osid); json_free(otrans); json_free(orec);
        return json_null();
    }
    /* if owner_session_id is None: with _quiet(None): owner_session_id = get_session_env(...) or None */
    if (dcr2_is_null(osid)) {
        json_t *qf = dcr2_quiet_factory(io, NULL, NULL, 0);
        if (qf) { json_free(osid); json_free(otrans); json_free(orec); return qf; }
        json_t *ga = dcr2_args();
        dcr2_arg_s(ga, "key", "HERMES_UI_SESSION_ID");
        dcr2_arg_s(ga, "default", "");
        json_t *env = dcr2_call(io, "get_session_env", ga);
        json_t *exit_raise = NULL;
        if (dcr2_is_raise(env)) {
            exit_raise = dcr2_quiet_exit(io, NULL, NULL, 0, env);
        } else {
            json_free(osid);
            osid = dcr2_truth(env) ? env : json_null();
            if (osid != env) json_free(env);
        }
        if (exit_raise) { json_free(osid); json_free(otrans); json_free(orec); return exit_raise; }
    }
    /* if owner_session_id and (owner_transport is None or owner_session_record is None):
     *     owner_transport, owner_session_record = _capture_gateway_steer_authority(owner_session_id) */
    if (dcr2_truth(osid) && (dcr2_is_null(otrans) || dcr2_is_null(orec))) {
        json_t *ca = dcr2_args();
        dcr2_arg_j(ca, "owner_session_id", osid);
        json_t *cap = dcr2_call(io, "_capture_gateway_steer_authority", ca);
        if (dcr2_is_raise(cap)) { json_free(osid); json_free(otrans); json_free(orec); return cap; }
        json_t *t = NULL, *r = NULL;
        json_t *ur = dcr2_unpack2(cap, &t, &r);
        json_free(cap);
        if (ur) { json_free(osid); json_free(otrans); json_free(orec); return ur; }
        json_free(otrans); json_free(orec);
        otrans = t; orec = r;
    }
    /* _raw_depth = getattr(child, "_delegate_depth", 1) */
    json_t *raw_depth = dcr2_getattr(child, "_delegate_depth", NULL);
    double depth = 0.0;
    if (dcr2_is_int(raw_depth)) {
        double d = dcr2_num(raw_depth) - 1.0;
        depth = d > 0.0 ? d : 0.0;
    }
    /* build the registry entry dict (insertion order). */
    json_t *entry = json_object();
    json_set(entry, "subagent_id", json_copy(sid));
    json_t *psid = dcr2_getattr(child, "_parent_subagent_id", NULL);
    json_t *parent_id = dcr2_str_or_none_seam(io, psid);
    if (dcr2_is_raise(parent_id)) { json_free(entry); json_free(osid); json_free(otrans); json_free(orec); return parent_id; }
    json_set(entry, "parent_id", parent_id);
    json_set(entry, "depth", json_number(depth));
    json_set(entry, "goal", goal ? json_copy(goal) : json_null());
    json_t *did = dcr2_getattr(child, "_delegation_id", NULL);
    json_t *deleg = dcr2_str_or_none_seam(io, did);
    if (dcr2_is_raise(deleg)) { json_free(entry); json_free(osid); json_free(otrans); json_free(orec); return deleg; }
    json_set(entry, "delegation_id", deleg);
    json_t *model = dcr2_getattr(child, "model", NULL);
    json_t *model_s = dcr2_str_or_none_seam(io, model);
    if (dcr2_is_raise(model_s)) { json_free(entry); json_free(osid); json_free(otrans); json_free(orec); return model_s; }
    json_set(entry, "model", model_s);
    json_t *now = dcr2_call(io, "time.time", dcr2_args());
    if (dcr2_is_raise(now)) { json_free(entry); json_free(osid); json_free(otrans); json_free(orec); return now; }
    json_set(entry, "started_at", now);
    json_set(entry, "status", json_string("running"));
    json_set(entry, "tool_count", json_number(0));
    json_set(entry, "agent", dcr2_norm_value(child));
    /* owner_agent_session_id = str(getattr(child,"_parent_session_id","") or "") or
     *                          str(getattr(parent,"session_id","") or "") or None */
    json_t *cpsid = dcr2_getattr(child, "_parent_session_id", NULL);
    char *c1 = dcr2_truth(cpsid) ? dcr2_pystr(cpsid) : dcr2_xfmt("");
    json_t *pssid = dcr2_getattr(parent, "session_id", NULL);
    char *c2 = dcr2_truth(pssid) ? dcr2_pystr(pssid) : dcr2_xfmt("");
    if (c1 && c1[0]) json_set(entry, "owner_agent_session_id", json_string(c1));
    else if (c2 && c2[0]) json_set(entry, "owner_agent_session_id", json_string(c2));
    else json_set(entry, "owner_agent_session_id", json_null());
    free(c1); free(c2);
    json_set(entry, "owner_session_id", json_copy(osid));
    json_set(entry, "owner_transport", json_copy(otrans));
    json_set(entry, "owner_session_record", json_copy(orec));
    json_t *ra = dcr2_args();
    json_set(ra, "entry", entry);  /* owns entry */
    json_t *reg = dcr2_call(io, "_register_subagent", ra);
    json_free(osid); json_free(otrans); json_free(orec);
    if (dcr2_is_raise(reg)) return reg;
    json_free(reg);
    return json_copy(sid);
}

/* =====================================================================
 * _create_isolated_worktree
 * ===================================================================== */
/* PoP: dcr2_create_isolated_worktree @ tools/delegate_tool_child_run.py:_create_isolated_worktree */
static json_t *dcr2_create_isolated_worktree(dcr2_io_t *io, const json_t *in, json_t *st)
{
    json_t *parent = dcr2_get(st, "parent_agent");
    json_t *parent_task_id = dcr2_get(in, "parent_task_id");
    json_t *subagent_id = dcr2_get(in, "subagent_id");
    /* if not _get_worktree_isolation(): return None */
    json_t *iso = dcr2_call(io, "_get_worktree_isolation", dcr2_args());
    if (dcr2_is_raise(iso)) return iso;
    int enabled = dcr2_truth(iso);
    json_free(iso);
    if (!enabled) return json_null();
    /* with _quiet("worktree isolation setup failed: %s"): */
    json_t *qf = dcr2_quiet_factory(io, "worktree isolation setup failed: %s", NULL, 0);
    if (qf) return qf;
    json_t *body_raise = NULL;
    json_t *result = NULL;
    json_t *parent_cwd = NULL;
    int done = 0;
    /* if not subagent_worktree.local_backend_active(): logger.debug(...); return None */
    json_t *lb = dcr2_call(io, "local_backend_active", dcr2_args());
    if (dcr2_is_raise(lb)) { body_raise = lb; goto exit; }
    int local = dcr2_truth(lb);
    json_free(lb);
    if (!local) {
        json_t *lr = dcr2_logger(io, "debug", "worktree isolation skipped: non-local terminal backend", json_array(), 0);
        if (dcr2_is_raise(lr)) { body_raise = lr; goto exit; }
        json_free(lr);
        result = json_null(); done = 1; goto exit;
    }
    /* _parent_cwd = None; with _quiet(None): _parent_cwd = get_session_cwd(parent_task_id) */
    parent_cwd = json_null();
    {
        json_t *qf2 = dcr2_quiet_factory(io, NULL, NULL, 0);
        if (qf2) { body_raise = qf2; goto exit; }
        json_t *ga = dcr2_args();
        dcr2_arg_j(ga, "task_id", parent_task_id);
        json_t *cwd = dcr2_call(io, "get_session_cwd", ga);
        if (dcr2_is_raise(cwd)) {
            json_t *ex = dcr2_quiet_exit(io, NULL, NULL, 0, cwd);
            if (ex) { body_raise = ex; goto exit; }
        } else {
            json_free(parent_cwd);
            parent_cwd = cwd;
        }
    }
    /* return create_subagent_worktree(_parent_cwd or _resolve_workspace_hint(parent), subagent_id=...) */
    {
        json_t *cwd_arg = NULL;
        if (dcr2_truth(parent_cwd)) {
            cwd_arg = json_copy(parent_cwd);
        } else {
            json_t *ha = dcr2_args();
            dcr2_arg_j(ha, "parent_agent", parent);
            cwd_arg = dcr2_call(io, "_resolve_workspace_hint", ha);
            if (dcr2_is_raise(cwd_arg)) { body_raise = cwd_arg; goto exit; }
        }
        json_t *ca = dcr2_args();
        dcr2_arg_j(ca, "cwd", cwd_arg);
        json_free(cwd_arg);
        dcr2_arg_j(ca, "subagent_id", subagent_id);
        json_t *cr = dcr2_call(io, "create_subagent_worktree", ca);
        if (dcr2_is_raise(cr)) { body_raise = cr; goto exit; }
        result = cr; done = 1;
    }
exit:
    json_free(parent_cwd);
    if (body_raise) {
        json_t *ex = dcr2_quiet_exit(io, "worktree isolation setup failed: %s", NULL, 0, body_raise);
        if (ex) return ex;
        return json_null();
    }
    if (done) return result;
    return json_null();
}

/* =====================================================================
 * _lease_child_credential
 * ===================================================================== */
/* find first entry in entries array matching a predicate; returns index or -1.
 * mode 0: e.id == cred_id ; mode 1: e.last_status != "dead" and serves(e). */
static json_t *dcr2_pool_entries(dcr2_io_t *io, json_t **out)
{
    json_t *r = dcr2_call(io, "pool.entries", dcr2_args());
    if (dcr2_is_raise(r)) return r;
    *out = r;
    return NULL;
}

/* PoP: dcr2_lease_child_credential @ tools/delegate_tool_child_run.py:_lease_child_credential */
static json_t *dcr2_lease_child_credential(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *child = dcr2_get(st, "child");
    json_t *pool = dcr2_getattr(child, "_credential_pool", NULL);
    if (dcr2_is_null(pool)) {
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    json_t *base_url = dcr2_getattr(child, "base_url", NULL);
    /* with child_profile_scope(child): */
    json_t *cm = dcr2_args();
    dcr2_arg_j(cm, "child", child);
    json_t *cmr = dcr2_cm_factory(io, "child_profile_scope", cm);
    if (cmr) return cmr;
    json_t *body_raise = NULL;
    json_t *leased_id = NULL;
    /* leased_cred_id = child_pool.acquire_lease() */
    json_t *acq = dcr2_call(io, "pool.acquire_lease", dcr2_args());
    if (dcr2_is_raise(acq)) { body_raise = acq; goto exit; }
    leased_id = acq;
    if (!dcr2_is_null(leased_id)) {
        /* with _quiet("Failed to bind child to leased credential: %s"): */
        json_t *qf = dcr2_quiet_factory(io, "Failed to bind child to leased credential: %s", NULL, 0);
        if (qf) { body_raise = qf; goto exit; }
        json_t *bind_raise = NULL;
        json_t *leased_entry = NULL;
        /* leased_entry = next((e for e in entries() if e.id == leased_cred_id), None) */
        json_t *entries = NULL;
        json_t *er = dcr2_pool_entries(io, &entries);
        if (er) { bind_raise = er; goto bind_exit; }
        if (!dcr2_is_arr(entries)) {
            bind_raise = dcr2_mkraise("TypeError", "'NoneType' object is not iterable");
            json_free(entries); goto bind_exit;
        }
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            if (!dcr2_hasattr(e, "id")) { bind_raise = dcr2_attr_raise(e, "id"); break; }
            json_t *eid = dcr2_dattr(e, "id");
            if (eid && leased_id && eid->type == leased_id->type) {
                int eq = 0;
                if (dcr2_is_str(eid) && dcr2_is_str(leased_id)) eq = strcmp(dcr2_str(eid), dcr2_str(leased_id)) == 0;
                else if (dcr2_is_num(eid) && dcr2_is_num(leased_id)) eq = dcr2_num(eid) == dcr2_num(leased_id);
                if (eq) { leased_entry = json_copy(e); break; }
            }
        }
        json_free(entries);
        if (bind_raise) goto bind_exit;
        /* if not _entry_serves_endpoint(leased_entry, base_url): */
        json_t *sa = dcr2_args();
        dcr2_arg_j(sa, "entry", leased_entry);
        dcr2_arg_j(sa, "base_url", base_url);
        json_t *serves = dcr2_call(io, "credential_pool_entry_serves_endpoint", sa);
        if (dcr2_is_raise(serves)) { bind_raise = serves; goto bind_exit; }
        int ok = dcr2_truth(serves);
        json_free(serves);
        if (!ok) {
            /* child_pool.release_lease(leased_cred_id) */
            json_t *rl = dcr2_args();
            dcr2_arg_j(rl, "cred_id", leased_id);
            json_t *rr = dcr2_call(io, "pool.release_lease", rl);
            if (dcr2_is_raise(rr)) { bind_raise = rr; goto bind_exit; }
            json_free(rr);
            /* leased_entry = next((e for e in entries() if e.last_status != "dead" and serves(e)), None) */
            json_t *entries2 = NULL;
            json_t *er2 = dcr2_pool_entries(io, &entries2);
            if (er2) { bind_raise = er2; goto bind_exit; }
            json_free(leased_entry);
            leased_entry = NULL;
            if (!dcr2_is_arr(entries2)) {
                bind_raise = dcr2_mkraise("TypeError", "'NoneType' object is not iterable");
                json_free(entries2); goto bind_exit;
            }
            for (size_t i = 0; i < json_len(entries2); i++) {
                json_t *e = json_get(entries2, i);
                if (!dcr2_hasattr(e, "last_status")) { bind_raise = dcr2_attr_raise(e, "last_status"); break; }
                json_t *ls = dcr2_dattr(e, "last_status");
                int dead = dcr2_is_str(ls) && strcmp(dcr2_str(ls), "dead") == 0;
                if (dead) continue;  /* short-circuit: serves seam not fired */
                json_t *sa2 = dcr2_args();
                dcr2_arg_j(sa2, "entry", e);
                dcr2_arg_j(sa2, "base_url", base_url);
                json_t *sv = dcr2_call(io, "credential_pool_entry_serves_endpoint", sa2);
                if (dcr2_is_raise(sv)) { bind_raise = sv; break; }
                int ok2 = dcr2_truth(sv);
                json_free(sv);
                if (ok2) { leased_entry = json_copy(e); break; }
            }
            json_free(entries2);
            if (bind_raise) goto bind_exit;
            /* leased_cred_id = acquire_lease(leased_entry.id) if leased_entry is not None else None */
            json_free(leased_id);
            if (leased_entry != NULL) {
                if (!dcr2_hasattr(leased_entry, "id")) { bind_raise = dcr2_attr_raise(leased_entry, "id"); leased_id = json_null(); goto bind_exit; }
                json_t *eid = dcr2_dattr(leased_entry, "id");
                json_t *aa = dcr2_args();
                dcr2_arg_j(aa, "cred_id", eid);
                json_t *acq2 = dcr2_call(io, "pool.acquire_lease", aa);
                if (dcr2_is_raise(acq2)) { bind_raise = acq2; leased_id = json_null(); goto bind_exit; }
                leased_id = acq2;
            } else {
                leased_id = json_null();
            }
        }
        /* if leased_entry is not None and hasattr(child, "_swap_credential"): child._swap_credential(leased_entry) */
        if (leased_entry != NULL && dcr2_hasattr(child, "_swap_credential")) {
            json_t *swap = dcr2_dattr(child, "_swap_credential");
            if (!dcr2_is_callable(swap)) { bind_raise = dcr2_not_callable_raise(swap); goto bind_exit; }
            const char *seam = dcr2_str(dcr2_get(swap, "__callable__"));
            json_t *swa = dcr2_args();
            dcr2_arg_j(swa, "entry", leased_entry);
            json_t *swr = dcr2_call(io, seam ? seam : "child._swap_credential", swa);
            if (dcr2_is_raise(swr)) { bind_raise = swr; goto bind_exit; }
            json_free(swr);
        }
bind_exit:
        json_free(leased_entry);
        if (bind_raise) {
            json_t *ex = dcr2_quiet_exit(io, "Failed to bind child to leased credential: %s", NULL, 0, bind_raise);
            if (ex) { body_raise = ex; goto exit; }
        }
    }
exit:
    if (body_raise) { json_free(leased_id); return body_raise; }
    json_t *out = json_array();
    json_append(out, json_copy(pool));
    json_append(out, leased_id ? leased_id : json_null());
    return out;
}

/* =====================================================================
 * _build_tool_trace
 * ===================================================================== */
/* PoP: dcr2_build_tool_trace @ tools/delegate_tool_child_run.py:_build_tool_trace */
static json_t *dcr2_build_tool_trace_v(dcr2_io_t *io, const json_t *messages)
{
    json_t *trace = json_array();
    if (!dcr2_is_arr(messages)) return trace;
    /* trace_by_id: array of {id, idx} pairs (insertion order; last wins). */
    json_t *by_id = json_array();
    for (size_t mi = 0; mi < json_len(messages); mi++) {
        json_t *msg = json_get(messages, mi);
        if (!dcr2_is_plain_dict(msg)) continue;
        json_t *role = dcr2_get(msg, "role");
        const char *rs = dcr2_str(role);
        if (rs && strcmp(rs, "assistant") == 0) {
            json_t *tcs = dcr2_get(msg, "tool_calls");
            if (!dcr2_truth(tcs)) continue;
            if (!dcr2_is_arr(tcs)) {
                char *m = dcr2_xfmt("'%s' object is not iterable", dcr2_typename(tcs));
                json_t *r = dcr2_mkraise("TypeError", m ? m : "not iterable"); free(m);
                json_free(trace); json_free(by_id); return r;
            }
            for (size_t ti = 0; ti < json_len(tcs); ti++) {
                json_t *tc = json_get(tcs, ti);
                if (!dcr2_is_plain_dict(tc)) {
                    json_t *r = dcr2_attr_raise(tc, "get");
                    json_free(trace); json_free(by_id); return r;
                }
                json_t *fn = dcr2_get(tc, "function");
                if (!fn) fn = NULL;  /* default {} */
                if (fn && !dcr2_is_plain_dict(fn)) {
                    json_t *r = dcr2_attr_raise(fn, "get");
                    json_free(trace); json_free(by_id); return r;
                }
                json_t *args = fn ? dcr2_get(fn, "arguments") : NULL;
                if (!args) args = NULL;  /* default "" */
                /* len(arguments) */
                size_t abytes;
                if (dcr2_is_str(args)) abytes = strlen(args->str_val ? args->str_val : "");
                else if (dcr2_is_arr(args) || dcr2_is_obj(args)) abytes = args->c.count;
                else if (dcr2_is_null(args)) abytes = 0;  /* default "" */
                else {
                    char *m = dcr2_xfmt("object of type '%s' has no len()", dcr2_typename(args));
                    json_t *r = dcr2_mkraise("TypeError", m ? m : "no len"); free(m);
                    json_free(trace); json_free(by_id); return r;
                }
                json_t *name = fn ? dcr2_get(fn, "name") : NULL;
                json_t *sa = dcr2_args();
                dcr2_arg_j(sa, "arguments", args);
                json_t *summary = dcr2_call(io, "_summarize_tool_arguments", sa);
                if (dcr2_is_raise(summary)) { json_free(trace); json_free(by_id); return summary; }
                json_t *entry = json_object();
                json_set(entry, "tool", name ? json_copy(name) : json_string("unknown"));
                json_set(entry, "args_bytes", json_number((double)abytes));
                json_set(entry, "input_summary", summary);
                size_t idx = json_len(trace);
                json_append(trace, entry);
                json_t *id = dcr2_get(tc, "id");
                if (dcr2_truth(id)) {
                    /* trace_by_id[id] = entry (last wins: replace existing). */
                    int found = -1;
                    for (size_t k = 0; k < json_len(by_id); k++) {
                        json_t *pair = json_get(by_id, k);
                        json_t *kid = dcr2_get(pair, "id");
                        if (kid && kid->type == id->type) {
                            int eq = 0;
                            if (dcr2_is_str(kid) && dcr2_is_str(id)) eq = strcmp(dcr2_str(kid), dcr2_str(id)) == 0;
                            else if (dcr2_is_num(kid) && dcr2_is_num(id)) eq = dcr2_num(kid) == dcr2_num(id);
                            if (eq) { found = (int)k; break; }
                        }
                    }
                    json_t *pair = json_object();
                    json_set(pair, "id", json_copy(id));
                    json_set(pair, "idx", json_number((double)idx));
                    if (found >= 0) json_array_set(by_id, (size_t)found, pair);
                    else json_append(by_id, pair);
                }
            }
        } else if (rs && strcmp(rs, "tool") == 0) {
            json_t *content = dcr2_get(msg, "content");
            json_t *ca = dcr2_args();
            dcr2_arg_j(ca, "content", content);
            json_t *content_s = dcr2_call(io, "_stringify_tool_content", ca);
            if (dcr2_is_raise(content_s)) { json_free(trace); json_free(by_id); return content_s; }
            size_t rbytes;
            if (dcr2_is_str(content_s)) rbytes = strlen(content_s->str_val ? content_s->str_val : "");
            else if (dcr2_is_arr(content_s) || dcr2_is_obj(content_s)) rbytes = content_s->c.count;
            else {
                char *m = dcr2_xfmt("object of type '%s' has no len()", dcr2_typename(content_s));
                json_t *r = dcr2_mkraise("TypeError", m ? m : "no len"); free(m);
                json_free(content_s); json_free(trace); json_free(by_id); return r;
            }
            json_t *la = dcr2_args();
            dcr2_arg_j(la, "content", content_s);
            json_t *looks = dcr2_call(io, "_looks_like_error_output", la);
            if (dcr2_is_raise(looks)) { json_free(content_s); json_free(trace); json_free(by_id); return looks; }
            int is_err = dcr2_truth(looks);
            json_free(looks);
            json_t *meta_status = json_string(is_err ? "error" : "ok");
            json_free(content_s);
            json_t *tc_id = dcr2_get(msg, "tool_call_id");
            long target = -1;
            if (dcr2_truth(tc_id)) {
                for (size_t k = 0; k < json_len(by_id); k++) {
                    json_t *pair = json_get(by_id, k);
                    json_t *kid = dcr2_get(pair, "id");
                    if (kid && kid->type == tc_id->type) {
                        int eq = 0;
                        if (dcr2_is_str(kid) && dcr2_is_str(tc_id)) eq = strcmp(dcr2_str(kid), dcr2_str(tc_id)) == 0;
                        else if (dcr2_is_num(kid) && dcr2_is_num(tc_id)) eq = dcr2_num(kid) == dcr2_num(tc_id);
                        if (eq) { target = (long)dcr2_num(dcr2_get(pair, "idx")); break; }
                    }
                }
            }
            if (target < 0 && json_len(trace) > 0) target = (long)json_len(trace) - 1;
            if (target >= 0) {
                json_t *te = json_get(trace, (size_t)target);
                json_set(te, "result_bytes", json_number((double)rbytes));
                json_set(te, "status", meta_status);
            } else {
                json_free(meta_status);
            }
        }
    }
    json_free(by_id);
    return trace;
}

static json_t *dcr2_build_tool_trace(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return dcr2_build_tool_trace_v(io, dcr2_get(in, "messages"));
}

/* =====================================================================
 * _validate_child_output_schema
 * ===================================================================== */
/* build a _SchemaOutcome projection dict. */
static json_t *dcr2_schema_outcome(json_t *schema, json_t *valid, json_t *errors, double retries)
{
    json_t *o = json_object();
    json_set(o, "schema", schema ? schema : json_null());
    json_set(o, "valid", valid ? valid : json_null());
    json_set(o, "errors", errors ? errors : json_array());
    json_set(o, "retries", json_number(retries));
    return o;
}

/* PoP: dcr2_validate_child_output_schema @ tools/delegate_tool_child_run.py:_validate_child_output_schema */
static json_t *dcr2_validate_child_output_schema(dcr2_io_t *io, const json_t *in, json_t *st)
{
    json_t *child = dcr2_get(st, "child");
    json_t *result = dcr2_get(st, "result");
    json_t *task_index = dcr2_get(in, "task_index");
    json_t *child_task_id = dcr2_get(in, "child_task_id");
    json_t *relay = dcr2_get(in, "relay_child_text");
    json_t *schema = dcr2_getattr(child, "_delegate_output_schema", NULL);
    if (!dcr2_is_plain_dict(schema)) {
        return dcr2_schema_outcome(schema ? json_copy(schema) : json_null(), NULL, NULL, 0);
    }
    /* _first_text = result.get("final_response") or "" */
    json_t *fr = dcr2_get(result, "final_response");
    json_t *first_text = dcr2_truth(fr) ? json_copy(fr) : json_string("");
    /* validate_output(_first_text, _output_schema) */
    json_t *va = dcr2_args();
    dcr2_arg_j(va, "text", first_text);
    dcr2_arg_j(va, "schema", schema);
    json_t *vr = dcr2_call(io, "validate_output", va);
    if (dcr2_is_raise(vr)) { json_free(first_text); return vr; }
    json_t *valid = NULL, *errors = NULL;
    json_t *ur = dcr2_unpack2(vr, &valid, &errors);
    json_free(vr);
    if (ur) { json_free(first_text); return ur; }
    /* if _schema_valid or not _first_text.strip() or result.get("interrupted", False): */
    int short_circuit = dcr2_truth(valid);
    if (!short_circuit) {
        if (!dcr2_is_str(first_text)) {
            json_t *r = dcr2_attr_raise(first_text, "strip");
            json_free(first_text); json_free(valid); json_free(errors); return r;
        }
        char *st_strip = dcr2_strip(first_text->str_val ? first_text->str_val : "");
        int empty = (st_strip && st_strip[0]) ? 0 : 1;
        free(st_strip);
        if (empty) short_circuit = 1;
    }
    if (!short_circuit) {
        json_t *interrupted = dcr2_get(result, "interrupted");
        if (dcr2_truth(interrupted)) short_circuit = 1;
    }
    if (short_circuit) {
        json_free(first_text);
        return dcr2_schema_outcome(json_copy(schema), valid, errors, 0);
    }
    json_free(first_text);
    /* _history = result.get("messages") if isinstance(list) else None */
    json_t *msgs = dcr2_get(result, "messages");
    json_t *history = dcr2_is_arr(msgs) ? msgs : NULL;
    /* try: with child_profile_scope(child), delegated_child_context(str(session_id or "")):
     *     _retry_result = child.run_conversation(...) */
    json_t *retry_result = NULL;
    json_t *retry_raise = NULL;
    {
        json_t *cm1 = dcr2_args();
        dcr2_arg_j(cm1, "child", child);
        json_t *cm1r = dcr2_cm_factory(io, "child_profile_scope", cm1);
        if (cm1r) { retry_raise = cm1r; goto retry_done; }
        /* delegated_child_context(str(getattr(child,"session_id","") or "")) */
        json_t *sid = dcr2_getattr(child, "session_id", NULL);
        char *sid_str = dcr2_truth(sid) ? dcr2_pystr(sid) : dcr2_xfmt("");
        json_t *cm2 = dcr2_args();
        dcr2_arg_s(cm2, "session_id", sid_str ? sid_str : "");
        free(sid_str);
        json_t *cm2r = dcr2_cm_factory(io, "delegated_child_context", cm2);
        if (cm2r) { retry_raise = cm2r; goto retry_done; }
        /* user_message = append_output_contract(build_retry_message(_schema_errors), _output_schema) */
        json_t *ba = dcr2_args();
        dcr2_arg_j(ba, "errors", errors);
        json_t *retry_msg = dcr2_call(io, "build_retry_message", ba);
        if (dcr2_is_raise(retry_msg)) { retry_raise = retry_msg; goto retry_done; }
        json_t *aa = dcr2_args();
        dcr2_arg_j(aa, "message", retry_msg);
        json_free(retry_msg);
        dcr2_arg_j(aa, "schema", schema);
        json_t *user_msg = dcr2_call(io, "append_output_contract", aa);
        if (dcr2_is_raise(user_msg)) { retry_raise = user_msg; goto retry_done; }
        /* child.run_conversation(user_message=..., task_id=..., stream_callback=..., conversation_history=...) */
        json_t *run = dcr2_getattr(child, "run_conversation", NULL);
        if (!dcr2_is_callable(run)) {
            json_free(user_msg);
            retry_raise = dcr2_is_null(run) ? dcr2_attr_raise(child, "run_conversation") : dcr2_not_callable_raise(run);
            goto retry_done;
        }
        const char *seam = dcr2_str(dcr2_get(run, "__callable__"));
        json_t *ra = dcr2_args();
        dcr2_arg_j(ra, "user_message", user_msg);
        json_free(user_msg);
        dcr2_arg_j(ra, "task_id", child_task_id);
        dcr2_arg_j(ra, "stream_callback", relay);
        if (history) json_set(ra, "conversation_history", json_copy(history));
        json_t *rr = dcr2_call(io, seam ? seam : "child.run_conversation", ra);
        if (dcr2_is_raise(rr)) { retry_raise = rr; goto retry_done; }
        retry_result = rr;
    }
retry_done:
    if (retry_raise) {
        /* except Exception as _retry_exc: logger.warning("Subagent %d schema-retry turn failed: %s", task_index, exc) */
        if (!dcr2_exc_is_exception(dcr2_kind(retry_raise))) {
            json_free(valid); json_free(errors); json_free(retry_result);
            return retry_raise;
        }
        json_t *la = json_array();
        json_append(la, task_index ? json_copy(task_index) : json_number(0));
        json_append(la, json_string(dcr2_raise_msg(retry_raise)));
        json_free(retry_raise);
        json_t *lr = dcr2_logger(io, "warning", "Subagent %d schema-retry turn failed: %s", la, 0);
        if (dcr2_is_raise(lr)) { json_free(valid); json_free(errors); json_free(retry_result); return lr; }
        json_free(lr);
    }
    /* if isinstance(_retry_result, dict): */
    if (dcr2_is_plain_dict(retry_result)) {
        json_t *rfr = dcr2_get(retry_result, "final_response");
        json_t *retry_text = dcr2_truth(rfr) ? json_copy(rfr) : json_string("");
        /* if _retry_text.strip(): result["final_response"] = _retry_text */
        if (!dcr2_is_str(retry_text)) {
            json_t *r = dcr2_attr_raise(retry_text, "strip");
            json_free(retry_text); json_free(valid); json_free(errors); json_free(retry_result); return r;
        }
        char *rts = dcr2_strip(retry_text->str_val ? retry_text->str_val : "");
        if (rts && rts[0]) json_set(result, "final_response", json_copy(retry_text));
        free(rts);
        /* try: result["api_calls"] = int(result.get("api_calls",0) or 0) + int(_retry_result.get("api_calls",0) or 0)
         * except (TypeError, ValueError): pass */
        {
            json_t *rc = dcr2_get(result, "api_calls");
            json_t *rc_v = dcr2_truth(rc) ? rc : NULL;
            json_t *i1;
            if (rc_v) i1 = dcr2_py_int(rc_v);
            else { json_t *z = json_number(0); i1 = dcr2_py_int(z); json_free(z); }
            json_t *ac = dcr2_get(retry_result, "api_calls");
            json_t *ac_v = dcr2_truth(ac) ? ac : NULL;
            json_t *i2;
            if (ac_v) i2 = dcr2_py_int(ac_v);
            else { json_t *z = json_number(0); i2 = dcr2_py_int(z); json_free(z); }
            if (!dcr2_is_raise(i1) && !dcr2_is_raise(i2)) {
                json_set(result, "api_calls", json_number(dcr2_num(i1) + dcr2_num(i2)));
            }
            json_free(i1); json_free(i2);
        }
        /* _retry_messages = _retry_result.get("messages") */
        json_t *rmsgs = dcr2_get(retry_result, "messages");
        if (dcr2_is_arr(rmsgs) && history != NULL) {
            json_t *start_node = dcr2_get(retry_result, "current_turn_user_idx");
            long start;
            if (dcr2_is_int(start_node)) {
                start = (long)dcr2_num(start_node);
            } else {
                /* max((i for i,m in enumerate(rmsgs) if m.get("role")=="user"), default=0) */
                long mx = -1;
                for (size_t i = 0; i < json_len(rmsgs); i++) {
                    json_t *m = json_get(rmsgs, i);
                    if (!dcr2_is_plain_dict(m)) {
                        json_t *r = dcr2_attr_raise(m, "get");
                        json_free(retry_text); json_free(valid); json_free(errors); json_free(retry_result); return r;
                    }
                    json_t *mrole = dcr2_get(m, "role");
                    if (dcr2_is_str(mrole) && strcmp(dcr2_str(mrole), "user") == 0) mx = (long)i;
                }
                start = mx >= 0 ? mx : 0;
            }
            /* result["messages"] = _history + _retry_messages[_start:] */
            long n = (long)json_len(rmsgs);
            long s = start;
            if (s < 0) s = n + s;
            if (s < 0) s = 0;
            if (s > n) s = n;
            json_t *newmsgs = json_copy(history);
            for (long i = s; i < n; i++) json_append(newmsgs, json_copy(json_get(rmsgs, (size_t)i)));
            json_set(result, "messages", newmsgs);
        }
        /* _schema_valid, _schema_errors = validate_output(_retry_text, _output_schema) */
        json_t *va2 = dcr2_args();
        dcr2_arg_j(va2, "text", retry_text);
        dcr2_arg_j(va2, "schema", schema);
        json_t *vr2 = dcr2_call(io, "validate_output", va2);
        json_free(retry_text);
        if (dcr2_is_raise(vr2)) { json_free(valid); json_free(errors); json_free(retry_result); return vr2; }
        json_t *v2 = NULL, *e2 = NULL;
        json_t *ur2 = dcr2_unpack2(vr2, &v2, &e2);
        json_free(vr2);
        if (ur2) { json_free(valid); json_free(errors); json_free(retry_result); return ur2; }
        json_free(valid); json_free(errors);
        valid = v2; errors = e2;
    }
    json_free(retry_result);
    return dcr2_schema_outcome(json_copy(schema), valid, errors, 1);
}

/* =====================================================================
 * _build_result_entry
 * ===================================================================== */
/* PoP: dcr2_build_result_entry @ tools/delegate_tool_child_run.py:_build_result_entry */
static json_t *dcr2_build_result_entry(dcr2_io_t *io, const json_t *in, json_t *st)
{
    json_t *child = dcr2_get(st, "child");
    json_t *result = dcr2_get(st, "result");
    json_t *task_index = dcr2_get(in, "task_index");
    json_t *duration = dcr2_get(in, "duration");
    json_t *schema = dcr2_get(in, "schema");  /* {schema, valid, errors, retries} */
    json_t *sch_schema = dcr2_get(schema, "schema");
    json_t *sch_valid = dcr2_get(schema, "valid");
    json_t *sch_errors = dcr2_get(schema, "errors");
    json_t *sch_retries = dcr2_get(schema, "retries");
    json_t *fail_raise = NULL;

    /* summary = result.get("final_response") or "" */
    json_t *fr = dcr2_get(result, "final_response");
    json_t *summary = dcr2_truth(fr) ? json_copy(fr) : json_string("");
    /* usable_summary = bool(summary) and summary.strip() != "(empty)" */
    int usable = 0;
    if (dcr2_truth(summary)) {
        if (!dcr2_is_str(summary)) { json_t *r = dcr2_attr_raise(summary, "strip"); json_free(summary); return r; }
        char *ss = dcr2_strip(summary->str_val ? summary->str_val : "");
        usable = (ss && strcmp(ss, "(empty)") != 0) ? 1 : 0;
        free(ss);
    }
    const char *status = NULL;
    const char *exit_reason = NULL;
    char *interrupt_note = dcr2_xfmt("");
    json_t *interrupted = dcr2_get(result, "interrupted");
    if (dcr2_truth(interrupted)) {
        status = "interrupted"; exit_reason = "interrupted";
        /* placeholders = {"", summary.strip(), "Operation interrupted."} */
        char *sum_strip = dcr2_strip(dcr2_is_str(summary) ? (summary->str_val ? summary->str_val : "") : "");
        /* partial = next((t for m in reversed(messages or []) if m.get("role")=="assistant"
         *                 and (t := flatten_message_text(m.get("content")).strip()) not in placeholders), "") */
        json_t *msgs = dcr2_get(result, "messages");
        json_t *mlist = dcr2_truth(msgs) ? msgs : NULL;
        char *partial = NULL;
        if (mlist && !dcr2_is_arr(mlist)) {
            char *m = dcr2_xfmt("'%s' object is not reversible", dcr2_typename(mlist));
            json_t *r = dcr2_mkraise("TypeError", m ? m : "not reversible"); free(m);
            free(sum_strip); free(interrupt_note); json_free(summary); return r;
        }
        if (mlist) {
            for (size_t k = json_len(mlist); k > 0; k--) {
                json_t *m = json_get(mlist, k - 1);
                if (!dcr2_is_plain_dict(m)) {
                    json_t *r = dcr2_attr_raise(m, "get");
                    free(sum_strip); free(interrupt_note); json_free(summary); return r;
                }
                json_t *mrole = dcr2_get(m, "role");
                if (!(dcr2_is_str(mrole) && strcmp(dcr2_str(mrole), "assistant") == 0)) continue;
                json_t *content = dcr2_get(m, "content");
                json_t *fa = dcr2_args();
                dcr2_arg_j(fa, "content", content);
                json_t *flat = dcr2_call(io, "flatten_message_text", fa);
                if (dcr2_is_raise(flat)) { free(sum_strip); free(interrupt_note); json_free(summary); return flat; }
                if (!dcr2_is_str(flat)) {
                    json_t *r = dcr2_attr_raise(flat, "strip");
                    json_free(flat); free(sum_strip); free(interrupt_note); json_free(summary); return r;
                }
                char *t = dcr2_strip(flat->str_val ? flat->str_val : "");
                json_free(flat);
                int in_ph = (t[0] == '\0') || (strcmp(t, sum_strip) == 0) || (strcmp(t, "Operation interrupted.") == 0);
                if (!in_ph) { partial = t; break; }
                free(t);
            }
        }
        if (partial && partial[0]) {
            free(interrupt_note);
            interrupt_note = sum_strip;  /* summary.strip() */
            sum_strip = NULL;
            json_free(summary);
            summary = json_string(partial);
        }
        free(partial);
        free(sum_strip);
        /* if (stale_after := result.get("_stale_stop_seconds")) is not None: */
        json_t *stale = dcr2_get(result, "_stale_stop_seconds");
        if (!dcr2_is_null(stale)) {
            status = "timeout"; exit_reason = "timeout";
            free(interrupt_note);
            if (!dcr2_is_number_or_bool(stale)) {
                json_t *r = dcr2_fmt_code_raise(stale, 'g');
                json_free(summary); return r;
            }
            char *gs = dcr2_fmt_g(dcr2_num(stale));
            interrupt_note = dcr2_xfmt("Subagent stopped making progress " DCR2_EMDASH " no activity for %ss "
                "(heartbeat stale threshold); it was stopped.", gs ? gs : "");
            free(gs);
        }
    } else {
        json_t *failed = dcr2_get(result, "failed");
        json_t *error = dcr2_get(result, "error");
        if (dcr2_truth(failed) || dcr2_truth(error)) {
            status = "failed"; exit_reason = "error";
        } else {
            json_t *completed = dcr2_get(result, "completed");
            exit_reason = dcr2_truth(completed) ? "completed" : "max_iterations";
            status = usable ? "completed" : "failed";
        }
    }
    /* entry dict (insertion order). */
    json_t *entry = json_object();
    json_set(entry, "task_index", task_index ? json_copy(task_index) : json_number(0));
    json_set(entry, "status", json_string(status));
    json_set(entry, "summary", json_copy(summary));
    json_t *api_calls = dcr2_get(result, "api_calls");
    json_set(entry, "api_calls", api_calls ? json_copy(api_calls) : json_number(0));
    json_set(entry, "duration_seconds", duration ? json_copy(duration) : json_number(0));
    json_t *model = dcr2_getattr(child, "model", NULL);
    json_t *model_s = dcr2_str_or_none_seam(io, model);
    if (dcr2_is_raise(model_s)) { fail_raise = model_s; goto fail; }
    json_set(entry, "model", model_s);
    json_set(entry, "exit_reason", json_string(exit_reason));
    json_set(entry, "truncated", json_bool(strcmp(exit_reason, "max_iterations") == 0));
    /* tokens */
    json_t *tokens = json_object();
    json_t *pt = dcr2_getattr(child, "session_prompt_tokens", NULL);
    json_t *pt_v = pt ? pt : json_number(0);
    json_t *in_num = dcr2_num_seam(io, pt_v);
    if (pt == NULL) json_free(pt_v);
    if (dcr2_is_raise(in_num)) { json_free(tokens); fail_raise = in_num; goto fail; }
    json_set(tokens, "input", in_num);
    json_t *ct = dcr2_getattr(child, "session_completion_tokens", NULL);
    json_t *ct_v = ct ? ct : json_number(0);
    json_t *out_num = dcr2_num_seam(io, ct_v);
    if (ct == NULL) json_free(ct_v);
    if (dcr2_is_raise(out_num)) { json_free(tokens); fail_raise = out_num; goto fail; }
    json_set(tokens, "output", out_num);
    json_set(entry, "tokens", tokens);
    /* tool_trace = _build_tool_trace(result.get("messages") or []) */
    json_t *msgs = dcr2_get(result, "messages");
    json_t *mlist = dcr2_truth(msgs) ? msgs : NULL;
    json_t *empty_msgs = NULL;
    if (!mlist) { empty_msgs = json_array(); mlist = empty_msgs; }
    json_t *tt = dcr2_build_tool_trace_v(io, mlist);
    json_free(empty_msgs);
    if (dcr2_is_raise(tt)) { fail_raise = tt; goto fail; }
    json_set(entry, "tool_trace", tt);
    json_t *role = dcr2_getattr(child, "_delegate_role", NULL);
    json_set(entry, "_child_role", role ? json_copy(role) : json_null());
    /* _child_cost_usd = float(_cost or 0.0) if isinstance(_cost,(int,float)) else 0.0 */
    json_t *cost = dcr2_getattr(child, "session_estimated_cost_usd", NULL);
    double cost_usd = 0.0;
    if (dcr2_is_number_or_bool(cost)) cost_usd = dcr2_truth(cost) ? dcr2_num(cost) : 0.0;
    json_set(entry, "_child_cost_usd", json_number(cost_usd));
    /* cost_usd = round(_child_cost_usd, 6) */
    json_set(entry, "cost_usd", dcr2_round_nd(cost_usd, 6));
    json_t *cost_status = dcr2_getattr(child, "session_cost_status", NULL);
    json_set(entry, "cost_status", (dcr2_is_str(cost_status) && dcr2_truth(cost_status)) ? json_copy(cost_status) : json_string("unknown"));
    /* _route = _route_report(child) */
    json_t *route = dcr2_route_report_seam(io, child);
    if (dcr2_is_raise(route)) { fail_raise = route; goto fail; }
    if (!dcr2_is_null(route)) json_set(entry, "route", route);
    else json_free(route);
    /* failure / interrupt-note fields */
    if (strcmp(status, "failed") == 0) {
        json_t *err = dcr2_get(result, "error");
        json_set(entry, "error", err ? json_copy(err) : json_string("Subagent did not produce a response."));
        json_t *frs = dcr2_get(result, "failure_reason");
        if (dcr2_is_str(frs) && dcr2_truth(frs)) json_set(entry, "failure_reason", json_copy(frs));
        json_t *resets = dcr2_get(result, "failure_resets_at");
        if (dcr2_is_number_or_bool(resets) && !dcr2_is_bool(resets)) json_set(entry, "failure_resets_at", json_copy(resets));
        json_t *ul = dcr2_get(result, "usage_limit");
        if (dcr2_is_plain_dict(ul)) json_set(entry, "usage_limit", json_copy(ul));
    } else if (interrupt_note && interrupt_note[0]) {
        json_set(entry, "error", json_string(interrupt_note));
    }
    /* schema outcome */
    if (dcr2_is_plain_dict(sch_schema)) {
        json_set(entry, "schema_valid", json_bool(dcr2_truth(sch_valid) ? true : false));
        if (dcr2_truth(sch_retries)) json_set(entry, "schema_retries", json_copy(sch_retries));
        if (!dcr2_truth(sch_valid) && dcr2_truth(sch_errors)) json_set(entry, "schema_errors", json_copy(sch_errors));
        /* schema.valid is False (identity) and usable_summary */
        if (dcr2_is_bool(sch_valid) && !sch_valid->bool_val && usable) {
            const char *retry_suffix = dcr2_truth(sch_retries) ? " (after 1 retry)" : "";
            char *note = dcr2_xfmt("Final answer does not satisfy the declared output_schema%s; `summary` is the "
                "child's raw, UNVALIDATED final text " DCR2_EMDASH " extract what you need from it yourself "
                "(see schema_errors) rather than re-running the task.", retry_suffix);
            json_set(entry, "schema_note", json_string(note ? note : ""));
            free(note);
        }
    }
    /* missed steer */
    json_t *missed = dcr2_get(result, "pending_steer");
    if (dcr2_is_str(missed)) {
        char *ms = dcr2_strip(missed->str_val ? missed->str_val : "");
        if (ms && ms[0]) {
            json_set(entry, "missed_steer", json_copy(missed));
            char *note = dcr2_xfmt("[steer did not land " DCR2_EMDASH " the subagent finished before it could "
                "be delivered: %s]", missed->str_val ? missed->str_val : "");
            char *newsum;
            if (dcr2_truth(summary)) newsum = dcr2_xfmt("%s\n\n%s", dcr2_str(summary) ? dcr2_str(summary) : "", note ? note : "");
            else newsum = dcr2_xfmt("%s", note ? note : "");
            json_set(entry, "summary", json_string(newsum ? newsum : ""));
            free(note); free(newsum);
        }
        free(ms);
    }
    free(interrupt_note);
    json_free(summary);
    return entry;

fail:
    free(interrupt_note);
    json_free(summary);
    json_free(entry);
    return fail_raise;
}

/* =====================================================================
 * _build_child_goal_message
 * ===================================================================== */
/* PoP: dcr2_build_child_goal_message @ tools/delegate_tool_child_run.py:_build_child_goal_message */
static json_t *dcr2_build_child_goal_message(dcr2_io_t *io, const json_t *in, json_t *st)
{
    json_t *child = dcr2_get(st, "child");
    json_t *goal = dcr2_get(in, "goal");
    json_t *images = dcr2_get(in, "images");
    const char *goal_s = dcr2_str(goal) ? dcr2_str(goal) : "";
    /* try: ... except Exception: logger.warning(...exc_info=True); return goal */
    json_t *body_raise = NULL;
    json_t *result = NULL;
    /* data_urls / urls / paths classification (s.startswith raises on non-str).
     * cfg is initialised HERE (before the first `goto except`): the `except:`
     * cleanup frees it, and two early gotos below jump over its old in-body
     * initialiser, leaving it indeterminate -> STATUS_HEAP_CORRUPTION. */
    json_t *data_urls = json_array();
    json_t *urls = json_array();
    json_t *paths = json_array();
    json_t *cfg = json_null();
    if (!dcr2_is_arr(images)) {
        char *m = dcr2_xfmt("'%s' object is not iterable", dcr2_typename(images));
        body_raise = dcr2_mkraise("TypeError", m ? m : "not iterable"); free(m);
        goto except;
    }
    for (size_t i = 0; i < json_len(images); i++) {
        json_t *s = json_get(images, i);
        if (!dcr2_is_str(s)) { body_raise = dcr2_attr_raise(s, "startswith"); goto except; }
        const char *sv = s->str_val ? s->str_val : "";
        int is_data = strncmp(sv, "data:image/", 11) == 0;
        int is_url = dcr2_is_image_url_v(s);
        if (is_data) json_append(data_urls, json_copy(s));
        if (is_url && !is_data) {
            /* s not in data_urls (data_urls only holds data: strings; a http url
             * is never a data url, so membership is always false here). */
            json_append(urls, json_copy(s));
        }
        if (!is_url) json_append(paths, json_copy(s));
    }
    /* cfg = None; with _quiet(None): cfg = load_config_readonly() -- cfg is
     * already initialised at the top of the function for the early-goto paths. */
    {
        json_t *qf = dcr2_quiet_factory(io, NULL, NULL, 0);
        if (qf) { body_raise = qf; goto except; }
        json_t *lc = dcr2_call(io, "load_config_readonly", dcr2_args());
        if (dcr2_is_raise(lc)) {
            json_t *ex = dcr2_quiet_exit(io, NULL, NULL, 0, lc);
            if (ex) { body_raise = ex; goto except; }
        } else {
            json_free(cfg);
            cfg = lc;
        }
    }
    /* mode = decide_image_input_mode(str(provider or ""), str(model or ""), cfg, requested_provider=str(...)) */
    {
        json_t *prov = dcr2_getattr(child, "provider", NULL);
        json_t *mod = dcr2_getattr(child, "model", NULL);
        json_t *rprov = dcr2_getattr(child, "requested_provider", NULL);
        char *prov_s = dcr2_truth(prov) ? dcr2_pystr(prov) : dcr2_xfmt("");
        char *mod_s = dcr2_truth(mod) ? dcr2_pystr(mod) : dcr2_xfmt("");
        char *rprov_s = dcr2_truth(rprov) ? dcr2_pystr(rprov) : dcr2_xfmt("");
        json_t *da = dcr2_args();
        dcr2_arg_s(da, "provider", prov_s ? prov_s : "");
        dcr2_arg_s(da, "model", mod_s ? mod_s : "");
        dcr2_arg_j(da, "cfg", cfg);
        dcr2_arg_s(da, "requested_provider", rprov_s ? rprov_s : "");
        free(prov_s); free(mod_s); free(rprov_s);
        json_t *mode = dcr2_call(io, "decide_image_input_mode", da);
        if (dcr2_is_raise(mode)) { body_raise = mode; goto except; }
        const char *ms = dcr2_str(mode);
        int native = ms && strcmp(ms, "native") == 0;
        json_free(mode);
        if (native) {
            /* parts, skipped = build_native_content_parts(goal, paths, urls) */
            json_t *ba = dcr2_args();
            dcr2_arg_s(ba, "goal", goal_s);
            json_set(ba, "paths", json_copy(paths));
            json_set(ba, "urls", json_copy(urls));
            json_t *br = dcr2_call(io, "build_native_content_parts", ba);
            if (dcr2_is_raise(br)) { body_raise = br; goto except; }
            json_t *parts = NULL, *skipped = NULL;
            json_t *ur = dcr2_unpack2(br, &parts, &skipped);
            json_free(br);
            if (ur) { body_raise = ur; goto except; }
            /* if skipped: logger.warning(... len(skipped), ", ".join(skipped[:3])) */
            if (dcr2_truth(skipped)) {
                size_t nskip = dcr2_is_arr(skipped) ? json_len(skipped) : 0;
                json_t *first3 = json_array();
                for (size_t i = 0; i < nskip && i < 3; i++) json_append(first3, json_copy(json_get(skipped, i)));
                json_t *js = dcr2_join(first3, ", ");
                json_free(first3);
                if (dcr2_is_raise(js)) { json_free(parts); json_free(skipped); body_raise = js; goto except; }
                json_t *la = json_array();
                json_append(la, json_number((double)nskip));
                json_append(la, js);
                json_t *lr = dcr2_logger(io, "warning", "delegate_task: skipped %d unreadable image(s) for subagent: %s", la, 0);
                if (dcr2_is_raise(lr)) { json_free(parts); json_free(skipped); body_raise = lr; goto except; }
                json_free(lr);
            }
            /* if data_urls: parts = (parts or [text part]) + [image_url parts] */
            if (dcr2_truth(data_urls)) {
                json_t *base;
                if (dcr2_truth(parts)) base = parts;
                else {
                    json_free(parts);
                    base = json_array();
                    json_t *tp = json_object();
                    json_set(tp, "type", json_string("text"));
                    json_set(tp, "text", json_string(goal_s));
                    json_append(base, tp);
                }
                for (size_t i = 0; i < json_len(data_urls); i++) {
                    json_t *ip = json_object();
                    json_set(ip, "type", json_string("image_url"));
                    json_t *iu = json_object();
                    json_set(iu, "url", json_copy(json_get(data_urls, i)));
                    json_set(ip, "image_url", iu);
                    json_append(base, ip);
                }
                parts = base;
            }
            /* return parts if any(p.get("type")=="image_url" for p in parts) else goal */
            int has_image = 0;
            if (dcr2_is_arr(parts)) {
                for (size_t i = 0; i < json_len(parts); i++) {
                    json_t *p = json_get(parts, i);
                    if (!dcr2_is_plain_dict(p)) { body_raise = dcr2_attr_raise(p, "get"); json_free(parts); json_free(skipped); goto except; }
                    json_t *pt = dcr2_get(p, "type");
                    if (dcr2_is_str(pt) && strcmp(dcr2_str(pt), "image_url") == 0) { has_image = 1; break; }
                }
            }
            json_free(skipped);
            if (has_image) { result = parts; goto done; }
            json_free(parts);
            result = json_string(goal_s);
            goto done;
        }
        /* non-native: if data_urls: logger.warning(... len(data_urls)) */
        if (dcr2_truth(data_urls)) {
            json_t *la = json_array();
            json_append(la, json_number((double)json_len(data_urls)));
            json_t *lr = dcr2_logger(io, "warning", "delegate_task: %d inline data-URL image(s) dropped for a non-vision subagent", la, 0);
            if (dcr2_is_raise(lr)) { body_raise = lr; goto except; }
            json_free(lr);
        }
        /* hints */
        json_t *hints = json_array();
        for (size_t i = 0; i < json_len(paths); i++) {
            json_t *p = json_get(paths, i);
            json_t *fa = dcr2_args();
            dcr2_arg_j(fa, "path", p);
            json_t *isf = dcr2_call(io, "os.path.isfile", fa);
            if (dcr2_is_raise(isf)) { json_free(hints); body_raise = isf; goto except; }
            int yes = dcr2_truth(isf);
            json_free(isf);
            if (yes) {
                char *h = dcr2_xfmt("[Image attached at: %s]", dcr2_str(p) ? dcr2_str(p) : "");
                json_append(hints, json_string(h ? h : ""));
                free(h);
            } else {
                json_t *la = json_array();
                json_append(la, json_copy(p));
                json_t *lr = dcr2_logger(io, "warning", "delegate_task: image path not found, not forwarded: %s", la, 0);
                if (dcr2_is_raise(lr)) { json_free(hints); body_raise = lr; goto except; }
                json_free(lr);
            }
        }
        for (size_t i = 0; i < json_len(urls); i++) {
            char *h = dcr2_xfmt("[Image attached: %s]", dcr2_str(json_get(urls, i)) ? dcr2_str(json_get(urls, i)) : "");
            json_append(hints, json_string(h ? h : ""));
            free(h);
        }
        if (!dcr2_truth(hints)) {
            json_free(hints);
            result = json_string(goal_s);
            goto done;
        }
        json_t *js = dcr2_join(hints, "\n");
        json_free(hints);
        if (dcr2_is_raise(js)) { body_raise = js; goto except; }
        char *full = dcr2_xfmt("%s\n\n%s\nUse vision_analyze to inspect these images.", goal_s, dcr2_str(js) ? dcr2_str(js) : "");
        json_free(js);
        result = json_string(full ? full : "");
        free(full);
        goto done;
    }
except:
    json_free(data_urls); json_free(urls); json_free(paths); json_free(cfg);
    if (body_raise) {
        if (!dcr2_exc_is_exception(dcr2_kind(body_raise))) return body_raise;
        json_free(body_raise);
        json_t *lr = dcr2_logger(io, "warning", "delegate_task: image forwarding failed; sending text-only goal", json_array(), 1);
        if (dcr2_is_raise(lr)) return lr;
        json_free(lr);
    }
    return json_string(goal_s);
done:
    json_free(data_urls); json_free(urls); json_free(paths); json_free(cfg);
    return result;
}

/* =====================================================================
 * _checkpointing (closure factory + transcribed _cb body)
 * ===================================================================== */
/* the inner._flush attribute: a callable marker carries a top-level "_flush"
 * key; an __attr__ double carries it in __attr__. */
static json_t *dcr2_flush_attr(const json_t *inner)
{
    if (!dcr2_is_obj(inner)) return NULL;
    if (json_obj_get(inner, "__attr__")) return dcr2_dattr(inner, "_flush");
    return json_obj_get(inner, "_flush");
}

/* PoP: dcr2_checkpointing @ tools/delegate_tool_child_run.py:_checkpointing */
static json_t *dcr2_checkpointing(dcr2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *inner = dcr2_get(in, "inner");
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("checkpointing.cb"));
    json_t *flush = dcr2_flush_attr(inner);
    if (flush) json_set(cb, "_flush", json_copy(flush));
    return cb;
}

/* the transcribed _cb body (invoked per event by the harness / oracle drive).
 * inner / checkpoint are the captured factory args (read from in). */
static json_t *dcr2_cb_invoke(dcr2_io_t *io, const json_t *in, const json_t *event)
{
    json_t *inner = dcr2_get(in, "inner");
    json_t *checkpoint = dcr2_get(in, "checkpoint");
    json_t *et = dcr2_get(event, "event_type");
    json_t *args = dcr2_get(event, "args");
    json_t *kwargs = dcr2_get(event, "kwargs");
    /* if callable(inner): inner(event_type, *args, **kwargs) */
    if (dcr2_is_callable(inner)) {
        const char *seam = dcr2_str(dcr2_get(inner, "__callable__"));
        json_t *a = dcr2_args();
        dcr2_arg_j(a, "event_type", et);
        json_set(a, "args", args ? json_copy(args) : json_array());
        json_set(a, "kwargs", kwargs ? json_copy(kwargs) : json_object());
        json_t *r = dcr2_call(io, seam ? seam : "inner.cb", a);
        if (dcr2_is_raise(r)) return r;
        json_free(r);
    }
    /* if event_type == "tool.completed": with _quiet("Durable subagent checkpoint failed: %s"): checkpoint() */
    const char *ets = dcr2_str(et);
    if (ets && strcmp(ets, "tool.completed") == 0) {
        json_t *qf = dcr2_quiet_factory(io, "Durable subagent checkpoint failed: %s", NULL, 0);
        if (qf) return qf;
        json_t *body_raise = NULL;
        if (dcr2_is_callable(checkpoint)) {
            const char *seam = dcr2_str(dcr2_get(checkpoint, "__callable__"));
            json_t *r = dcr2_call(io, seam ? seam : "checkpoint.cb", dcr2_args());
            if (dcr2_is_raise(r)) body_raise = r; else json_free(r);
        } else {
            body_raise = dcr2_not_callable_raise(checkpoint);
        }
        if (body_raise) {
            json_t *ex = dcr2_quiet_exit(io, "Durable subagent checkpoint failed: %s", NULL, 0, body_raise);
            if (ex) return ex;
        }
    }
    return json_null();
}