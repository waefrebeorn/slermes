/* conversation_loop_flow.c — FLOW port of agent/conversation_loop.py (LIVE
 * authority). Ports 24 UNCLAIMED functions: the continuation-overlap KMP leaf
 * (_continuation_overlap_length), the partial-turn result builder
 * (_partial_turn_result), the display-metadata force-redaction
 * (_redact_display_metadata) and failed-turn metadata (_failed_turn_display_metadata),
 * the durable failed-turn boundary closer (_close_durable_failed_turn), the MoA
 * guidance peel (_peel_moa_guidance) and inline-turn decode (_decode_inline_moa_turn),
 * the credential-refresh profile pick (_credential_refresh_profile), the 💡 guidance
 * printer (_print_guidance), the context-engine hook override probe
 * (_engine_overrides_hook), the run-budget wrap-up injection
 * (_maybe_inject_run_budget_wrapup), the Ollama context-limit error
 * (_ollama_context_limit_error), the system-prompt persist (_persist_system_prompt),
 * the pinned-tools restore (_restore_pinned_tools), the Bot Chat tools refresh
 * (_refresh_bot_chat_tools), the Bot Chat prompt-stale probe (_bot_chat_prompt_stale),
 * the usage-window refusal (_usage_window_refusal), the fallback-restart arm
 * (_arm_fallback_restart), the preflight-timeout result (_preflight_timeout_result),
 * and the turn-loop phase machinery (_run_phase, _run_api_retry_loop,
 * _finalize_from_state, _turn_without_loop, _codex_app_server_turn).
 *
 * Claimed siblings (_sync_failover_system_message, _billing_or_entitlement_message,
 * _billing_block_dict, _restore_or_build_system_prompt, ...) and all foreign I/O
 * enter through the cl_io_t universal seam; ported siblings compose for REAL
 * (_partial_turn_result inside _preflight_timeout_result, _redact_display_metadata
 * inside _failed_turn_display_metadata, _failed_turn_display_metadata inside
 * _close_durable_failed_turn, _run_phase inside _run_api_retry_loop,
 * _finalize_from_state / _codex_app_server_turn inside _turn_without_loop).
 *
 * Decision sequences only: Python truthiness, `or`/`and` short-circuits, `is None`
 * identity vs truthy tests, kwarg/operand evaluation order, dict insertion order,
 * and except-Exception catch scopes are transcribed per call site. A scripted
 * BaseException (SystemExit / KeyboardInterrupt / GeneratorExit) propagates past
 * every except Exception guard exactly as live (full builtin exception-parent
 * table below).
 *
 * Value model (shared with tests/oracle_conversation_loop_flow.py):
 *   - attribute doubles {"__attr__": {...}} <-> SimpleNamespace; the state window
 *     keeps the wrapper form and is mutated in place.
 *   - class-tagged doubles {"__class__": "_LoopState"|...} give type() identity
 *     (the AttributeError type-name uses the tag).
 *   - callable doubles {"__callable__": "<seam>"} (callable() is True); an optional
 *     "__func__" carries an identity ref for _engine_overrides_hook.
 *   - identity refs {"__ref__": "<id>"} resolve through the harness id registry.
 *   - raise specs {"__raise__": "<ClassName>", "message": str(e)}.
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - str.strip()/splitlines() use the ASCII White_Space set (incl \x1c-\x1f for
 *     strip; splitlines boundaries \n \r \v \f \x1c \x1d \x1e). Non-ASCII text is
 *     out of scope. The 💡 (U+1F4A1), ✕ (U+2715) and em-dash (U+2014) literals are
 *     hex-escaped.
 *   - int()/float() coercions accept number / bool / signed-decimal-string with
 *     ASCII-space padding; int() strips the NARROWER ws set (no \x1c-\x1f).
 *   - json.loads in _restore_pinned_tools is pre-validated against the strict
 *     Python grammar (leading zeros / invalid escapes / NaN / trailing comma are
 *     rejected with CPython JSONDecodeError messages); the fixture corpus stays
 *     in that verified domain.
 *   - _continuation_overlap_length operates on ASCII codepoints (byte==char).
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified "agent/conversation_loop_flow.c".
 * Every symbol is static cl_*.
 *
 * Gate: bash tests/run_one_oracle.sh conversation_loop_flow \
 *           oracle_conversation_loop_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── live module constants (byte-pinned to agent/conversation_loop.py and the
 *    foreign modules the unclaimed defs import) ── */
#define CL_MIN_CONTINUATION_OVERLAP 32
#define CL_MINIMUM_CONTEXT_LENGTH 64000
#define CL_FAILED_TURN_ERROR_MAX_CHARS 2000
#define CL_DB_PERSISTED_MARKER "_db_persisted"
#define CL_BOT_CHAT_TITLE "Bot Chat"
#define CL_PRE_LLM_BLOCK_EXIT_REASON "blocked_by_plugin_pre_llm_call"
/* U+1F4A1 💡, U+2715 ✕, U+2014 em dash (hex-escaped). */
#define CL_BULB "\xf0\x9f\x92\xa1"
#define CL_XMARK "\xe2\x9c\x95"
#define CL_EMDASH "\xe2\x80\x94"
/* RUN_BUDGET_WRAPUP_NOTICE (agent/conversation_loop.py). */
#define CL_RUN_BUDGET_WRAPUP_NOTICE \
    "[SYSTEM NOTICE " CL_EMDASH " run time budget nearly exhausted] Run time budget nearly exhausted. " \
    "Stop new discovery/verification work now. Produce the required final deliverable " \
    "(answer/JSON/summary) from the state you already have, completing only mandatory writes."
/* OPEN_TURN_TAIL_ROLES = frozenset({"user","tool"}); membership only. */
/* _LATCHED_VERDICT_FIELDS: handle_api_error latches _provider_overflow_recovery_pending. */

/* ── universal seam ── */
typedef struct cl_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cl_io_t;

/* ── truthiness / accessors ── */
static int cl_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cl_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cl_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cl_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cl_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cl_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cl_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cl_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cl_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cl_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static json_t *cl_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* isinstance(x, dict): a PLAIN object, never an __attr__ double. */
static int cl_is_plain_dict(const json_t *v)
{
    return cl_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

/* isinstance(x, int): a number OR a bool (bool is an int subclass). Integral
 * floats read as int in libjson (no int/float identity); the fixture domain
 * keeps integral numbers integral-int. */
static int cl_is_int(const json_t *v)
{
    if (cl_is_bool(v)) return 1;
    if (!cl_is_num(v)) return 0;
    double d = v->num_val;
    return isfinite(d) && d == (double)(long long)d;
}

/* callable(x): only a {"__callable__": ...} double is callable here. */
static int cl_is_callable(const json_t *v)
{
    return cl_is_obj(v) && json_obj_get(v, "__callable__") != NULL;
}

/* class tag of a double wrapper (NULL when untagged). */
static const char *cl_class(const json_t *v)
{
    return cl_str(cl_get(v, "__class__"));
}

static const char *cl_typename(const json_t *v)
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
            const char *tag = cl_class(v);
            if (tag) return tag;
            return json_obj_get(v, "__attr__") ? "types.SimpleNamespace" : "dict";
        }
        default: return "object";
    }
}

/* ── state-window helpers ──
 * A state carrier st.<key> is a double wrapper {"__attr__": {...}}; the inner
 * object is the mutable __attr__ dict. cl_self auto-vivifies so writes land.
 * cl_dattr / cl_dset read / write an attribute of a double wrapper. */
static json_t *cl_self(json_t *st, const char *key)
{
    json_t *node = json_obj_get(st, key);
    if (!node || node->type != JSON_OBJECT) {
        node = json_object();
        json_set(st, key, node);
    }
    json_t *inner = json_obj_get(node, "__attr__");
    if (!inner || inner->type != JSON_OBJECT) {
        inner = json_object();
        json_set(node, "__attr__", inner);
    }
    return inner;
}

static json_t *cl_dattr(const json_t *d, const char *name)
{
    return cl_get(cl_get(d, "__attr__"), name);
}

static void cl_dset(const json_t *d, const char *name, json_t *val)
{
    json_t *inner = cl_get(d, "__attr__");
    if (inner && inner->type == JSON_OBJECT) json_set(inner, name, val);
    else json_free(val);
}

/* ── raise specs ── */
static int cl_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cl_kind(const json_t *r)
{
    const char *k = cl_str(cl_get(r, "__raise__"));
    return k ? k : "";
}

static const char *cl_raise_msg(const json_t *r)
{
    const char *m = cl_str(cl_get(r, "message"));
    return m ? m : "";
}

static int cl_is_base(const char *kind)
{
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0;
}

static json_t *cl_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *cl_xfmt(const char *fmt, ...)
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

/* ── FULL builtin exception-parent hierarchy (py3.14). Walk to test
 *    except Exception / except <Class> ancestry. BaseException trio (and
 *    asyncio.CancelledError) parent to BaseException ONLY, so they pass
 *    through every except Exception. ── */
typedef struct { const char *name; const char *parent; } cl_exc_row;
static const cl_exc_row CL_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"JSONDecodeError", "ValueError"},
    {"Warning", "Exception"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {NULL, NULL},
};

static const char *cl_exc_parent(const char *name)
{
    for (size_t i = 0; CL_EXC_PARENTS[i].name; i++)
        if (strcmp(CL_EXC_PARENTS[i].name, name) == 0) return CL_EXC_PARENTS[i].parent;
    /* unknown synthetic kind: treat as a direct Exception subclass (the oracle
     * builds type(kind, (Exception,), {})). */
    return "Exception";
}

/* issubclass(kind, base): walk the parent chain (bounded). */
static int cl_exc_is_instance(const char *kind, const char *base)
{
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, base) == 0) return 1;
        const char *p = cl_exc_parent(cur);
        if (!p || strcmp(cur, p) == 0) break;
        snprintf(cur, sizeof(cur), "%s", p);
        if (strcmp(cur, "BaseException") == 0) return strcmp(base, "BaseException") == 0;
    }
    return 0;
}

/* except Exception catches kind iff Exception is in its ancestry. */
static int cl_exc_is_exception(const char *kind)
{
    return cl_exc_is_instance(kind, "Exception");
}

/* ── universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. ── */
static json_t *cl_call(cl_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cl_xfmt("no seam installed for %s", name);
        r = cl_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *cl_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides (harness strips nulls
 * recursively; oracle norm_args drops None). */
static void cl_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void cl_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void cl_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void cl_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* ── string helpers (ASCII domain) ── */
/* str.strip White_Space (ASCII): space \t \n \r \v \f \x1c \x1d \x1e \x1f. */
static int cl_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
/* int() strips the NARROWER set (no \x1c-\x1f). */
static int cl_is_int_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *cl_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cl_is_ws((unsigned char)s[a])) a++;
    while (b > a && cl_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *cl_lower(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    out[n] = '\0';
    return out;
}

static int cl_startswith(const char *s, const char *prefix)
{
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* str.removeprefix (ASCII). Caller frees. */
static char *cl_removeprefix(const char *s, const char *prefix)
{
    if (!s) s = "";
    if (prefix && cl_startswith(s, prefix)) return cl_xfmt("%s", s + strlen(prefix));
    return cl_xfmt("%s", s);
}

/* Python str() projection (domain: str / integral number / bool / None). */
static char *cl_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cl_xfmt("None");
    if (v->type == JSON_BOOL) return cl_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d) return cl_xfmt("%.0f", d);
        return cl_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cl_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : cl_xfmt("");
}

/* {:,} thousands grouping of an integral value (caller frees). */
static char *cl_thousands(double d)
{
    long long n = (long long)d;
    int neg = n < 0;
    unsigned long long u = neg ? (unsigned long long)(-(n + 1)) + 1ULL : (unsigned long long)n;
    char digits[32];
    int k = 0;
    if (u == 0) digits[k++] = '0';
    while (u > 0) { digits[k++] = (char)('0' + (u % 10)); u /= 10; }
    char buf[64];
    int b = 0;
    if (neg) buf[b++] = '-';
    for (int i = k - 1; i >= 0; i--) {
        buf[b++] = digits[i];
        int remaining = i;
        if (remaining > 0 && remaining % 3 == 0) buf[b++] = ',';
    }
    buf[b] = '\0';
    return cl_xfmt("%s", buf);
}

/* length-bounded string node (libjson has only json_string). */
static json_t *cl_strn(const char *s, size_t n)
{
    char *tmp = (char *)malloc(n + 1);
    if (!tmp) return json_string("");
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    json_t *r = json_string(tmp);
    free(tmp);
    return r;
}

/* str.splitlines() — ASCII boundaries \n \r \r\n \v \f \x1c \x1d \x1e.
 * Returns a NEW array of NEW strings. A trailing boundary does NOT yield an
 * extra empty element (Python semantics). */
static json_t *cl_splitlines(const char *s)
{
    json_t *out = json_array();
    if (!s) return out;
    size_t n = strlen(s), i = 0, start = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        int isb = (c == '\n' || c == '\r' || c == '\v' || c == '\f'
                   || c == 0x1c || c == 0x1d || c == 0x1e);
        if (isb) {
            json_append(out, cl_strn(s + start, i - start));
            if (c == '\r' && i + 1 < n && s[i + 1] == '\n') i++;
            i++;
            start = i;
        } else {
            i++;
        }
    }
    if (start < n) json_append(out, cl_strn(s + start, n - start));
    return out;
}

/* ── int(value) / float(value) projections (restricted ASCII domain). Each
 *    returns a NEW number node or a raise spec. ── */
static json_t *cl_pyint(const json_t *v)
{
    if (cl_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (cl_is_num(v)) {
        double d = v->num_val;
        if (!isfinite(d))
            return cl_mkraise("OverflowError", "cannot convert float infinity to integer");
        return json_number(trunc(d));
    }
    if (cl_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && cl_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && cl_is_int_ws((unsigned char)s[b - 1])) b--;
        size_t i = a;
        int neg = 0;
        if (i < b && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
        size_t dstart = i;
        int ndig = 0, prev_digit = 0;
        while (i < b) {
            char c = s[i];
            if (c >= '0' && c <= '9') { ndig++; prev_digit = 1; i++; }
            else if (c == '_' && prev_digit && i + 1 < b && s[i + 1] >= '0' && s[i + 1] <= '9') { prev_digit = 0; i++; }
            else break;
        }
        if (ndig == 0 || i != b) {
            char *m = cl_xfmt("invalid literal for int() with base 10: '%s'", s);
            json_t *r = cl_mkraise("ValueError", m ? m : "");
            free(m);
            return r;
        }
        double acc = 0.0;
        for (size_t k = dstart; k < i; k++) if (s[k] != '_') acc = acc * 10.0 + (s[k] - '0');
        return json_number(neg ? -acc : acc);
    }
    char *m = cl_xfmt("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                      cl_typename(v));
    json_t *r = cl_mkraise("TypeError", m ? m : "");
    free(m);
    return r;
}

static int cl_float_digits(const char **p)
{
    const char *s = *p;
    int n = 0;
    for (;;) {
        if (*s < '0' || *s > '9') break;
        s++; n++;
        if (s[0] == '_' && s[1] >= '0' && s[1] <= '9') { s++; continue; }
        if (s[0] == '_') break;
    }
    *p = s;
    return n;
}

static int cl_parse_float_str(const char *t, double *out)
{
    if (!t) return 2;
    char *s = cl_strip(t);
    if (!s) return 2;
    const char *p = s;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    char low[16];
    size_t li = 0;
    for (const char *q = p; *q && li < 15; q++) low[li++] = (char)tolower((unsigned char)*q);
    low[li] = '\0';
    if (!strcmp(low, "inf") || !strcmp(low, "infinity")) { *out = neg ? -INFINITY : INFINITY; free(s); return 0; }
    if (!strcmp(low, "nan")) { *out = NAN; free(s); return 0; }
    const char *start = p;
    int intd = cl_float_digits(&p);
    int fracd = 0;
    if (*p == '.') { p++; fracd = cl_float_digits(&p); }
    if (intd == 0 && fracd == 0) { free(s); return 2; }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        int ed = cl_float_digits(&p);
        if (ed == 0) { free(s); return 2; }
    }
    if (*p != '\0') { free(s); return 2; }
    size_t n = (size_t)(p - start);
    char *clean = (char *)malloc(n + 2);
    if (!clean) { free(s); return 2; }
    size_t k = 0;
    if (neg) clean[k++] = '-';
    for (const char *q = start; q < p; q++) if (*q != '_') clean[k++] = *q;
    clean[k] = '\0';
    char *end = NULL;
    errno = 0;
    double v = strtod(clean, &end);
    int ok = (end && *end == '\0' && end != clean);
    free(clean);
    free(s);
    if (!ok) return 2;
    *out = v;
    return 0;
}

static json_t *cl_pyfloat(const json_t *v)
{
    if (cl_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (cl_is_num(v)) return json_number(v->num_val);
    if (cl_is_str(v)) {
        double d = 0.0;
        if (cl_parse_float_str(v->str_val ? v->str_val : "", &d) == 0) return json_number(d);
        char *m = cl_xfmt("could not convert string to float: '%s'", v->str_val ? v->str_val : "");
        json_t *r = cl_mkraise("ValueError", m ? m : "");
        free(m);
        return r;
    }
    char *m = cl_xfmt("float() argument must be a string or a real number, not '%s'", cl_typename(v));
    json_t *r = cl_mkraise("TypeError", m ? m : "");
    free(m);
    return r;
}

/* ── strict json.loads (Python grammar) for _restore_pinned_tools. A scanner
 *    validates the grammar and reports CPython JSONDecodeError messages; on
 *    success libjson json_parse builds the value (the verified domain agrees).
 *    Non-finite constants (NaN/Infinity) are OUT of the fixture domain and
 *    rejected here. Returns the parsed node or a JSONDecodeError raise spec. ── */
typedef struct {
    const char *s;
    size_t n, i;
    char msg[96];
    size_t errpos;
    int err;
} cl_jscan;

static void cl_jerr(cl_jscan *j, const char *msg, size_t pos)
{
    if (!j->err) { j->err = 1; snprintf(j->msg, sizeof(j->msg), "%s", msg); j->errpos = pos; }
}
static void cl_jws(cl_jscan *j)
{
    while (j->i < j->n) {
        char c = j->s[j->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') j->i++;
        else break;
    }
}
static int cl_jvalue(cl_jscan *j);
static int cl_jstring(cl_jscan *j)
{
    /* assumes s[i]=='"'; consumes the string, validates escapes/control chars */
    size_t start = j->i;
    j->i++;
    while (j->i < j->n) {
        unsigned char c = (unsigned char)j->s[j->i];
        if (c == '"') { j->i++; return 0; }
        if (c == '\\') {
            j->i++;
            if (j->i >= j->n) { cl_jerr(j, "Unterminated string starting at", start); return -1; }
            char e = j->s[j->i];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't') { j->i++; }
            else if (e == 'u') {
                j->i++;
                for (int k = 0; k < 4; k++) {
                    if (j->i >= j->n || !isxdigit((unsigned char)j->s[j->i])) {
                        cl_jerr(j, "Invalid \\escape", start); return -1;
                    }
                    j->i++;
                }
            } else { cl_jerr(j, "Invalid \\escape", start); return -1; }
        } else if (c < 0x20) {
            cl_jerr(j, "Invalid control character at", j->i); return -1;
        } else { j->i++; }
    }
    cl_jerr(j, "Unterminated string starting at", start);
    return -1;
}
static int cl_jnumber(cl_jscan *j)
{
    size_t start = j->i;
    if (j->i < j->n && j->s[j->i] == '-') j->i++;
    /* int part: 0 | [1-9][0-9]* */
    if (j->i < j->n && j->s[j->i] == '0') { j->i++; }
    else if (j->i < j->n && j->s[j->i] >= '1' && j->s[j->i] <= '9') {
        while (j->i < j->n && j->s[j->i] >= '0' && j->s[j->i] <= '9') j->i++;
    } else { cl_jerr(j, "Expecting value", start); return -1; }
    /* fraction: consume '.' only when followed by >=1 digit (regex backtrack) */
    if (j->i + 1 < j->n && j->s[j->i] == '.' && j->s[j->i + 1] >= '0' && j->s[j->i + 1] <= '9') {
        j->i++;
        while (j->i < j->n && j->s[j->i] >= '0' && j->s[j->i] <= '9') j->i++;
    }
    /* exponent: consume only when (optional sign +) >=1 digit follows */
    if (j->i < j->n && (j->s[j->i] == 'e' || j->s[j->i] == 'E')) {
        size_t k = j->i + 1;
        if (k < j->n && (j->s[k] == '+' || j->s[k] == '-')) k++;
        if (k < j->n && j->s[k] >= '0' && j->s[k] <= '9') {
            j->i = k;
            while (j->i < j->n && j->s[j->i] >= '0' && j->s[j->i] <= '9') j->i++;
        }
    }
    return 0;
}
static int cl_jobject(cl_jscan *j)
{
    j->i++; /* { */
    cl_jws(j);
    if (j->i < j->n && j->s[j->i] == '}') { j->i++; return 0; }
    for (;;) {
        cl_jws(j);
        if (j->i >= j->n || j->s[j->i] != '"') {
            cl_jerr(j, "Expecting property name enclosed in double quotes", j->i); return -1;
        }
        if (cl_jstring(j) != 0) return -1;
        cl_jws(j);
        if (j->i >= j->n || j->s[j->i] != ':') { cl_jerr(j, "Expecting ':' delimiter", j->i); return -1; }
        j->i++;
        if (cl_jvalue(j) != 0) return -1;
        cl_jws(j);
        if (j->i < j->n && j->s[j->i] == '}') { j->i++; return 0; }
        if (j->i >= j->n || j->s[j->i] != ',') { cl_jerr(j, "Expecting ',' delimiter", j->i); return -1; }
        size_t comma_pos = j->i;
        j->i++;
        cl_jws(j);
        if (j->i < j->n && j->s[j->i] == '}') {
            cl_jerr(j, "Illegal trailing comma before end of object", comma_pos); return -1;
        }
    }
}
static int cl_jarray(cl_jscan *j)
{
    j->i++; /* [ */
    cl_jws(j);
    if (j->i < j->n && j->s[j->i] == ']') { j->i++; return 0; }
    for (;;) {
        if (cl_jvalue(j) != 0) return -1;
        cl_jws(j);
        if (j->i < j->n && j->s[j->i] == ']') { j->i++; return 0; }
        if (j->i >= j->n || j->s[j->i] != ',') { cl_jerr(j, "Expecting ',' delimiter", j->i); return -1; }
        size_t comma_pos = j->i;
        j->i++;
        cl_jws(j);
        if (j->i < j->n && j->s[j->i] == ']') {
            cl_jerr(j, "Illegal trailing comma before end of array", comma_pos); return -1;
        }
    }
}
static int cl_jvalue(cl_jscan *j)
{
    cl_jws(j);
    if (j->i >= j->n) { cl_jerr(j, "Expecting value", j->i); return -1; }
    char c = j->s[j->i];
    if (c == '"') return cl_jstring(j);
    if (c == '{') return cl_jobject(j);
    if (c == '[') return cl_jarray(j);
    if (c == '-' || (c >= '0' && c <= '9')) return cl_jnumber(j);
    if (j->n - j->i >= 4 && strncmp(j->s + j->i, "true", 4) == 0) { j->i += 4; return 0; }
    if (j->n - j->i >= 5 && strncmp(j->s + j->i, "false", 5) == 0) { j->i += 5; return 0; }
    if (j->n - j->i >= 4 && strncmp(j->s + j->i, "null", 4) == 0) { j->i += 4; return 0; }
    cl_jerr(j, "Expecting value", j->i);
    return -1;
}
static json_t *cl_json_loads(const char *text)
{
    cl_jscan j;
    j.s = text ? text : "";
    j.n = strlen(j.s);
    j.i = 0;
    j.err = 0;
    j.msg[0] = '\0';
    j.errpos = 0;
    if (cl_jvalue(&j) != 0) goto fail;
    cl_jws(&j);
    if (j.i != j.n) { cl_jerr(&j, "Extra data", j.i); goto fail; }
    {
        json_t *v = json_parse(j.s, NULL);
        if (!v) { cl_jerr(&j, "Expecting value", 0); goto fail; }
        return v;
    }
fail:
    {
        size_t pos = j.errpos;
        long line = 1, lastnl = -1;
        for (size_t k = 0; k < pos && k < j.n; k++)
            if (j.s[k] == '\n') { line++; lastnl = (long)k; }
        long col = (long)pos - lastnl;
        char *m = cl_xfmt("%s: line %ld column %ld (char %zu)", j.msg, line, col, pos);
        json_t *r = cl_mkraise("JSONDecodeError", m ? m : j.msg);
        free(m);
        return r;
    }
}

/* ── required attribute read: getattr(obj, name) with NO default raises
 *    AttributeError when absent. Returns the value, or a raise spec the caller
 *    must propagate. Present-null is returned as a JSON_NULL node. ── */
static json_t *cl_req_attr(const json_t *d, const char *name)
{
    json_t *inner = cl_get(d, "__attr__");
    json_t *v = cl_get(inner, name);
    if (!v) {
        char *m = cl_xfmt("'%s' object has no attribute '%s'", cl_typename(d), name);
        json_t *r = cl_mkraise("AttributeError", m ? m : "");
        free(m);
        return r;
    }
    return v; /* borrowed */
}

/* ── logger seam helpers. The oracle projects BaseException positional args to
 *    str(x); C builds the message string from the raise spec. ── */
static json_t *cl_log(cl_io_t *io, const char *level, const char *msg, json_t *args_arr, int exc_info)
{
    json_t *a = cl_args();
    cl_arg_s(a, "msg", msg);
    if (args_arr) json_set(a, "args", args_arr); else json_set(a, "args", json_array());
    if (exc_info) cl_arg_b(a, "exc_info", 1);
    char *nm = cl_xfmt("logger.%s", level);
    json_t *r = cl_call(io, nm ? nm : "logger.debug", a);
    free(nm);
    return r;
}

/* ════════════════════════ ported functions ════════════════════════ */

/* PoP: cl_continuation_overlap_length @ agent/conversation_loop.py:_continuation_overlap_length */
static json_t *cl_continuation_overlap_length(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *prev = cl_str(cl_get(in, "previous"));
    const char *cont = cl_str(cl_get(in, "continuation"));
    if (!prev) prev = "";
    if (!cont) cont = "";
    size_t lp = strlen(prev), lc = strlen(cont);
    if (lp < CL_MIN_CONTINUATION_OVERLAP || lc < CL_MIN_CONTINUATION_OVERLAP)
        return json_number(0);
    int *pl = (int *)calloc(lc, sizeof(int));
    if (!pl) return json_number(0);
    size_t matched = 0;
    for (size_t index = 1; index < lc; index++) {
        while (matched && cont[index] != cont[matched]) matched = (size_t)pl[matched - 1];
        if (cont[index] == cont[matched]) { matched++; pl[index] = (int)matched; }
    }
    matched = 0;
    long last_index = (long)lp - 1;
    long result = 0;
    for (size_t index = 0; index < lp; index++) {
        char c = prev[index];
        while (matched && c != cont[matched]) matched = (size_t)pl[matched - 1];
        if (c == cont[matched]) {
            matched++;
            if (matched == lc) {
                if ((long)index == last_index) { result = (long)matched; goto done; }
                matched = (size_t)pl[matched - 1];
            }
        }
    }
    result = (matched >= CL_MIN_CONTINUATION_OVERLAP) ? (long)matched : 0;
done:
    free(pl);
    return json_number((double)result);
}

/* PoP: cl_partial_turn_result @ agent/conversation_loop.py:_partial_turn_result */
static json_t *cl_partial_turn_result(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *fr = cl_get(in, "final_response");
    const json_t *msgs = cl_get(in, "messages");
    const json_t *flags = cl_get(in, "flags");
    json_t *o = json_object();
    json_set(o, "final_response", cl_copy_or_null(fr));
    json_set(o, "messages", cl_copy_or_null(msgs));
    json_set(o, "completed", json_bool(false));
    json_set(o, "api_calls", cl_copy_or_null(cl_get(in, "api_call_count")));
    json_set(o, "error", cl_copy_or_null(fr));
    json_set(o, "partial", json_bool(true));
    /* **flags: overwrite in place (json_set keeps first position, last value). */
    if (cl_is_obj(flags)) {
        for (size_t i = 0; i < flags->c.count; i++)
            json_set(o, flags->c.keys[i], json_copy(flags->c.items[i]));
    }
    return o;
}

/* recursive force-redaction helper for _redact_display_metadata. Returns a NEW
 * node or a raise spec (propagated). */
static json_t *cl_redact_value(cl_io_t *io, const json_t *v)
{
    if (cl_is_str(v)) {
        json_t *a = cl_args();
        cl_arg_j(a, "arg0", v);
        cl_arg_b(a, "force", 1);
        return cl_call(io, "redact_sensitive_text", a);
    }
    if (cl_is_obj(v)) {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *rv = cl_redact_value(io, v->c.items[i]);
            if (cl_is_raise(rv)) { json_free(o); return rv; }
            json_set(o, v->c.keys[i], rv);
        }
        return o;
    }
    if (cl_is_arr(v)) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *rv = cl_redact_value(io, v->c.items[i]);
            if (cl_is_raise(rv)) { json_free(a); return rv; }
            json_append(a, rv);
        }
        return a;
    }
    return cl_copy_or_null(v);
}

/* PoP: cl_redact_display_metadata @ agent/conversation_loop.py:_redact_display_metadata */
static json_t *cl_redact_display_metadata(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *md = cl_get(in, "metadata");
    if (!cl_is_obj(md)) return json_object();
    return cl_redact_value(io, md);
}

/* PoP: cl_peel_moa_guidance @ agent/conversation_loop.py:_peel_moa_guidance */
static json_t *cl_peel_moa_guidance(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *a = cl_args();
    cl_arg_j(a, "messages", cl_get(in, "messages"));
    cl_arg_j(a, "guidance", cl_get(in, "guidance"));
    return cl_call(io, "peel_reference_guidance", a);
}

/* PoP: cl_credential_refresh_profile @ agent/conversation_loop.py:_credential_refresh_profile */
static json_t *cl_credential_refresh_profile(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *agent = cl_self(st, "agent");
    /* (getattr(agent,"provider","") or "").strip().lower() */
    const char *prov_raw = cl_str(cl_get(agent, "provider"));
    char *prov_or = cl_xfmt("%s", (prov_raw && prov_raw[0]) ? prov_raw : "");
    char *prov_strip = cl_strip(prov_or);
    char *prov = cl_lower(prov_strip ? prov_strip : "");
    free(prov_or); free(prov_strip);
    /* str(getattr(agent,"base_url","") or "") */
    json_t *bu = cl_get(agent, "base_url");
    char *bu_str = cl_truth(bu) ? cl_pystr(bu) : cl_xfmt("");
    json_t *a = cl_args();
    cl_arg_s(a, "hook", "refresh_runtime_credentials");
    cl_arg_s(a, "provider", prov);
    cl_arg_s(a, "base_url", bu_str);
    free(prov); free(bu_str);
    json_t *profiles = cl_call(io, "route_profiles", a);
    if (cl_is_raise(profiles)) return profiles;
    json_t *ret = NULL;
    if (cl_truth(profiles) && cl_is_arr(profiles) && json_len(profiles) > 0)
        ret = json_copy(json_get(profiles, 0));
    json_free(profiles);
    return ret;
}

/* PoP: cl_decode_inline_moa_turn @ agent/conversation_loop.py:_decode_inline_moa_turn */
static json_t *cl_decode_inline_moa_turn(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *um = cl_get(in, "user_message");
    const json_t *persist = cl_get(in, "persist_user_message");
    json_t *a = cl_args();
    cl_arg_j(a, "arg0", um);
    json_t *r = cl_call(io, "decode_moa_turn", a);
    if (cl_is_raise(r)) {
        /* except Exception: pass — BaseException propagates. */
        if (cl_exc_is_exception(cl_kind(r))) { json_free(r); goto unchanged; }
        return r;
    }
    /* unpack exactly 2; a wrong shape raises (caught) -> unchanged. */
    if (cl_is_arr(r) && json_len(r) == 2) {
        json_t *dmsg = json_get(r, 0);
        json_t *dcfg = json_get(r, 1);
        if (!cl_is_null(dcfg)) {
            json_t *out = json_array();
            json_append(out, json_copy(dmsg));
            json_append(out, json_copy(dcfg));
            json_append(out, cl_is_null(persist) ? json_copy(dmsg) : json_copy(persist));
            json_free(r);
            return out;
        }
    }
    json_free(r);
unchanged:
    {
        json_t *out = json_array();
        json_append(out, cl_copy_or_null(um));
        json_append(out, json_null());
        json_append(out, cl_copy_or_null(persist));
        return out;
    }
}

/* PoP: cl_print_guidance @ agent/conversation_loop.py:_print_guidance */
static json_t *cl_print_guidance(cl_io_t *io, const json_t *in, json_t *st)
{
    const char *message = cl_str(cl_get(in, "message"));
    if (!message || !message[0]) return json_bool(false);
    json_t *agent = cl_self(st, "agent");
    json_t *lp = cl_req_attr(cl_get(st, "agent"), "log_prefix");
    if (cl_is_raise(lp)) return lp;
    const char *prefix = cl_str(lp);
    if (!prefix) prefix = "";
    json_t *lines = cl_splitlines(message);
    json_t *ret = json_bool(true);
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = cl_str(json_get(lines, i));
        char *text = cl_xfmt("%s   " CL_BULB " %s", prefix, line ? line : "");
        json_t *a = cl_args();
        cl_arg_s(a, "arg0", text);
        cl_arg_b(a, "force", 1);
        cl_arg_b(a, "diagnostic", 1);
        free(text);
        json_t *r = cl_call(io, "vprint", a);
        if (cl_is_raise(r)) { json_free(lines); return r; }
        json_free(r);
    }
    json_free(lines);
    return ret;
}

/* PoP: cl_engine_overrides_hook @ agent/conversation_loop.py:_engine_overrides_hook */
static json_t *cl_engine_overrides_hook(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *engine = cl_get(in, "engine");
    const char *name = cl_str(cl_get(in, "name"));
    if (!name) name = "";
    json_t *hook = cl_dattr(engine, name);
    if (cl_is_null(engine) || !cl_is_callable(hook)) return json_bool(false);
    /* try: import ContextEngine; getattr(_CE, name) — both gate to True on fail. */
    if (cl_truth(cl_get(in, "import_fails"))) return json_bool(true);
    if (!cl_truth(cl_get(in, "ce_has_hook"))) return json_bool(true);
    /* getattr(hook, "__func__", None) is not getattr(_CE, name): identity is
     * modeled by the ce_func ref id. */
    json_t *func = cl_get(hook, "__func__");
    const char *ref = cl_str(cl_get(func, "__ref__"));
    int is_ce = (ref && strcmp(ref, "ce_func") == 0);
    return json_bool(is_ce ? false : true);
}

/* PoP: cl_maybe_inject_run_budget_wrapup @ agent/conversation_loop.py:_maybe_inject_run_budget_wrapup */
static json_t *cl_maybe_inject_run_budget_wrapup(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *agent = cl_self(st, "agent");
    json_t *messages = cl_get(st, "messages");
    json_t *budget = cl_get(agent, "run_budget_seconds");
    json_t *started = cl_get(agent, "_run_budget_started_at");
    json_t *injected = cl_get(agent, "_run_budget_wrapup_injected");
    /* if not budget or not started or injected or ((time.time()-started) < 0.8*float(budget)) */
    if (!cl_truth(budget) || !cl_truth(started) || cl_truth(injected))
        return json_bool(false);
    json_t *t1 = cl_call(io, "time", cl_args());
    if (cl_is_raise(t1)) return t1;
    json_t *fb = cl_pyfloat(budget);
    if (cl_is_raise(fb)) { json_free(t1); return fb; }
    double elapsed = cl_num(t1) - cl_num(started);
    int below = elapsed < 0.8 * cl_num(fb);
    json_free(t1);
    if (below) { json_free(fb); return json_bool(false); }
    /* from agent.context_compressor import _DB_PERSISTED_MARKER (constant) */
    if (!cl_is_arr(messages)) { json_free(fb); return json_bool(false); }
    for (size_t k = json_len(messages); k > 0; k--) {
        json_t *msg = json_get(messages, k - 1);
        if (!cl_is_plain_dict(msg)) continue;
        const char *role = cl_str(cl_get(msg, "role"));
        if (!role || strcmp(role, "tool") != 0) continue;
        if (cl_truth(cl_get(msg, CL_DB_PERSISTED_MARKER))) { json_free(fb); return json_bool(false); }
        json_t *existing = cl_get(msg, "content");
        if (cl_is_str(existing)) {
            char *nc = cl_xfmt("%s\n\n%s", existing->str_val ? existing->str_val : "", CL_RUN_BUDGET_WRAPUP_NOTICE);
            json_set(msg, "content", json_string(nc ? nc : ""));
            free(nc);
        } else {
            /* msg["content"] = [*(existing or []), {"type":"text","text":NOTICE}] */
            json_t *block = json_object();
            json_set(block, "type", json_string("text"));
            json_set(block, "text", json_string(CL_RUN_BUDGET_WRAPUP_NOTICE));
            json_t *arr = json_array();
            if (cl_is_arr(existing)) {
                for (size_t i = 0; i < json_len(existing); i++) json_append(arr, json_copy(json_get(existing, i)));
            } else if (!cl_truth(existing)) {
                /* existing or [] -> [] */
            } else if (cl_is_obj(existing)) {
                for (size_t i = 0; i < existing->c.count; i++) json_append(arr, json_string(existing->c.keys[i]));
            } else {
                /* *int / *bool -> TypeError caught -> return False */
                json_free(arr); json_free(block); json_free(fb);
                return json_bool(false);
            }
            json_append(arr, block);
            json_set(msg, "content", arr);
        }
        cl_dset(cl_get(st, "agent"), "_run_budget_wrapup_injected", json_bool(true));
        /* logger.info(fmt, float(budget), time.time()-started) — time fires again */
        json_t *t2 = cl_call(io, "time", cl_args());
        if (cl_is_raise(t2)) { json_free(fb); return t2; }
        json_t *args_arr = json_array();
        json_append(args_arr, json_copy(fb));
        json_append(args_arr, json_number(cl_num(t2) - cl_num(started)));
        json_free(t2);
        json_t *lr = cl_log(io, "info",
            "Run budget wrap-up notice injected (budget=%.0fs, elapsed=%.0fs)", args_arr, 0);
        json_free(fb);
        if (cl_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(true);
    }
    json_free(fb);
    return json_bool(false);
}

/* PoP: cl_ollama_context_limit_error @ agent/conversation_loop.py:_ollama_context_limit_error */
static json_t *cl_ollama_context_limit_error(cl_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = cl_self(st, "agent");
    json_t *runtime_ctx = cl_get(agent, "_ollama_num_ctx");
    json_t *tools = cl_get(agent, "tools");
    double req = cl_num(cl_get(in, "request_tokens"));
    if (!cl_truth(tools) || !cl_is_int(runtime_ctx)
        || !(0 < cl_num(runtime_ctx) && cl_num(runtime_ctx) < CL_MINIMUM_CONTEXT_LENGTH))
        return NULL;
    const char *model_raw = cl_str(cl_get(agent, "model"));
    const char *model = (model_raw && model_raw[0]) ? model_raw : "the selected model";
    const char *prov_raw = cl_str(cl_get(agent, "provider"));
    const char *prov = (prov_raw && prov_raw[0]) ? prov_raw : "unknown";
    const char *bu_raw = cl_str(cl_get(agent, "base_url"));
    const char *bu = (bu_raw && bu_raw[0]) ? bu_raw : "unknown base URL";
    json_t *sid = cl_get(agent, "session_id");
    char *sid_str = cl_pystr(cl_truth(sid) ? sid : NULL);
    if (!cl_truth(sid)) { free(sid_str); sid_str = cl_xfmt("none"); }
    double tool_count = cl_is_arr(tools) ? (double)json_len(tools) : 0.0;
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(model));
    json_append(args_arr, json_string(prov));
    json_append(args_arr, json_string(bu));
    json_append(args_arr, json_copy(runtime_ctx));
    json_append(args_arr, json_number(CL_MINIMUM_CONTEXT_LENGTH));
    json_append(args_arr, json_number(req));
    json_append(args_arr, json_number(tool_count));
    json_append(args_arr, json_string(sid_str));
    json_t *lr = cl_log(io, "warning",
        "Ollama runtime context too small for Hermes tool use: model=%s provider=%s base_url=%s "
        "runtime_context=%d minimum_context=%d estimated_request_tokens=%d tool_count=%d session=%s",
        args_arr, 0);
    if (cl_is_raise(lr)) { free(sid_str); return lr; }
    json_free(lr);
    char *ctx_g = cl_thousands(cl_num(runtime_ctx));
    char *min_g = cl_thousands(CL_MINIMUM_CONTEXT_LENGTH);
    char *ret = cl_xfmt(
        "Ollama loaded `%s` with only %s tokens of runtime context, but Hermes "
        "needs at least %s tokens for reliable tool use.\n\n"
        "Increase the Ollama context for this model and restart/reload the model before trying "
        "again. A known-good starting point is 65,536 tokens. In Hermes config, set "
        "`model.ollama_num_ctx: 65536` (and `model.context_length: 65536` if you also override the "
        "displayed model context). If you manage the model through an Ollama Modelfile, set "
        "`PARAMETER num_ctx 65536` there instead.",
        model, ctx_g ? ctx_g : "", min_g ? min_g : "");
    free(ctx_g); free(min_g); free(sid_str);
    json_t *out = json_string(ret ? ret : "");
    free(ret);
    return out;
}

/* PoP: cl_persist_system_prompt @ agent/conversation_loop.py:_persist_system_prompt */
static json_t *cl_persist_system_prompt(cl_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = cl_self(st, "agent");
    const char *failure_message = cl_str(cl_get(in, "failure_message"));
    int persist_tools = cl_truth(cl_get(in, "persist_tools"));
    /* if not agent._session_db: return (direct attr read raises when absent) */
    json_t *sdb = cl_req_attr(cl_get(st, "agent"), "_session_db");
    if (cl_is_raise(sdb)) return sdb;
    if (!cl_truth(sdb)) return NULL;
    /* try: store = agent_interface(agent); (store.sessions if store else db).update_system_prompt(...) */
    json_t *ai_args = cl_args();
    cl_arg_j(ai_args, "agent", cl_get(st, "agent"));
    json_t *store = cl_call(io, "agent_interface", ai_args);
    json_t *exc = NULL;
    if (cl_is_raise(store)) { exc = store; goto handle; }
    {
        json_t *sid = cl_req_attr(cl_get(st, "agent"), "session_id");
        if (cl_is_raise(sid)) { exc = sid; json_free(store); goto handle; }
        json_t *csp = cl_req_attr(cl_get(st, "agent"), "_cached_system_prompt");
        if (cl_is_raise(csp)) { exc = csp; json_free(store); goto handle; }
        json_t *a = cl_args();
        cl_arg_j(a, "session_id", sid);
        cl_arg_j(a, "prompt", csp);
        json_t *r = cl_call(io, "update_system_prompt", a);
        if (cl_is_raise(r)) { exc = r; json_free(store); goto handle; }
        json_free(r);
        if (persist_tools) {
            json_t *pa = cl_args();
            cl_arg_j(pa, "agent", cl_get(st, "agent"));
            json_t *pr = cl_call(io, "persist_agent_tool_names", pa);
            if (cl_is_raise(pr)) { exc = pr; json_free(store); goto handle; }
            json_free(pr);
        }
    }
    json_free(store);
    return NULL;
handle:
    {
        /* except Exception as exc: logger.warning(failure_message, agent.session_id, exc) */
        if (!cl_exc_is_exception(cl_kind(exc))) return exc; /* BaseException passthrough */
        json_t *sid = cl_req_attr(cl_get(st, "agent"), "session_id");
        json_t *args_arr = json_array();
        if (cl_is_raise(sid)) { json_free(exc); return sid; }
        json_append(args_arr, json_copy(sid));
        json_append(args_arr, json_string(cl_raise_msg(exc)));
        json_t *lr = cl_log(io, "warning", failure_message ? failure_message : "", args_arr, 0);
        json_free(exc);
        if (cl_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
}

/* PoP: cl_restore_pinned_tools @ agent/conversation_loop.py:_restore_pinned_tools */
static json_t *cl_restore_pinned_tools(cl_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = cl_self(st, "agent");
    const json_t *session_row = cl_get(in, "session_row");
    /* built_for_this_surface = agent_tool_names(agent) — outside any try */
    json_t *ba = cl_args();
    cl_arg_j(ba, "agent", cl_get(st, "agent"));
    json_t *built = cl_call(io, "agent_tool_names", ba);
    if (cl_is_raise(built)) return built;
    /* saved_tools = session_row.get("tool_names") if session_row else None */
    json_t *saved = NULL;
    if (cl_truth(session_row)) saved = cl_get(session_row, "tool_names");
    /* pin = json.loads(saved_tools) if saved_tools else None; except ValueError -> None */
    json_t *pin = NULL;
    if (cl_truth(saved)) {
        if (cl_is_str(saved)) {
            json_t *parsed = cl_json_loads(saved->str_val ? saved->str_val : "");
            if (cl_is_raise(parsed)) {
                if (cl_exc_is_instance(cl_kind(parsed), "ValueError")) { json_free(parsed); pin = NULL; }
                else { json_free(built); return parsed; }
            } else pin = parsed;
        } else {
            /* json.loads(non-str) raises TypeError (not ValueError) -> propagates */
            char *m = cl_xfmt("the JSON object must be str, bytes or bytearray, not '%s'", cl_typename(saved));
            json_t *r = cl_mkraise("TypeError", m ? m : "");
            free(m);
            json_free(built);
            return r;
        }
    }
    /* try: if pin: restore... elif session_row is not None and not _persist_disabled: persist... */
    json_t *exc = NULL;
    if (cl_truth(pin)) {
        json_t *a = cl_args();
        cl_arg_j(a, "agent", cl_get(st, "agent"));
        cl_arg_j(a, "pin", pin);
        json_t *r = cl_call(io, "restore_agent_tool_prefix", a);
        if (cl_is_raise(r)) exc = r; else json_free(r);
    } else if (!cl_is_null(session_row) && !cl_truth(cl_get(agent, "_persist_disabled"))) {
        json_t *a = cl_args();
        cl_arg_j(a, "agent", cl_get(st, "agent"));
        json_t *r = cl_call(io, "persist_agent_tool_names", a);
        if (cl_is_raise(r)) exc = r; else json_free(r);
    }
    json_free(pin);
    if (exc) {
        if (!cl_exc_is_exception(cl_kind(exc))) { json_free(built); return exc; }
        json_t *lr = cl_log(io, "debug", "tool prefix restore skipped", NULL, 1);
        json_free(exc);
        if (cl_is_raise(lr)) { json_free(built); return lr; }
        json_free(lr);
    }
    return built;
}

/* PoP: cl_refresh_bot_chat_tools @ agent/conversation_loop.py:_refresh_bot_chat_tools */
static json_t *cl_refresh_bot_chat_tools(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *agent = cl_self(st, "agent");
    json_t *policy = cl_call(io, "active_policy", cl_args());
    if (cl_is_raise(policy)) return policy;
    json_t *platform = cl_get(agent, "platform");
    const char *plat = cl_str(platform);
    int plat_ok = (plat && (strcmp(plat, "desktop") == 0 || strcmp(plat, "tui") == 0));
    if (cl_is_null(policy) && !plat_ok) { json_free(policy); return NULL; }
    json_t *exc = NULL;
    if (!cl_is_null(policy)) {
        /* enabled_override = list(policy.toolsets) */
        json_t *ts = cl_req_attr(policy, "toolsets");
        if (cl_is_raise(ts)) { exc = ts; goto handle; }
        if (!cl_is_arr(ts)) {
            char *m = cl_xfmt("'%s' object is not iterable", cl_typename(ts));
            exc = cl_mkraise("TypeError", m ? m : ""); free(m); goto handle;
        }
        json_t *a = cl_args();
        cl_arg_j(a, "agent", cl_get(st, "agent"));
        cl_arg_j(a, "enabled_override", ts);
        cl_arg_b(a, "quiet_mode", 1);
        cl_arg_b(a, "content_aware", 1);
        json_t *r = cl_call(io, "refresh_agent_mcp_tools", a);
        if (cl_is_raise(r)) { exc = r; goto handle; }
        json_free(r);
        json_free(policy);
        return NULL;
    }
    {
        json_t *ea = cl_args();
        cl_arg_j(ea, "platform", platform);
        json_t *enabled = cl_call(io, "load_enabled_toolsets", ea);
        if (cl_is_raise(enabled)) { exc = enabled; goto handle; }
        json_t *da = cl_args();
        json_t *disabled = cl_call(io, "load_disabled_toolsets", da);
        if (cl_is_raise(disabled)) { exc = disabled; json_free(enabled); goto handle; }
        json_t *a = cl_args();
        cl_arg_j(a, "agent", cl_get(st, "agent"));
        cl_arg_j(a, "enabled_override", enabled);
        cl_arg_j(a, "disabled_override", disabled);
        cl_arg_b(a, "quiet_mode", 1);
        cl_arg_b(a, "content_aware", 1);
        json_free(enabled); json_free(disabled);
        json_t *r = cl_call(io, "refresh_agent_mcp_tools", a);
        if (cl_is_raise(r)) { exc = r; goto handle; }
        json_free(r);
    }
    json_free(policy);
    return NULL;
handle:
    json_free(policy);
    if (!cl_exc_is_exception(cl_kind(exc))) return exc;
    {
        json_t *sid = cl_req_attr(cl_get(st, "agent"), "session_id");
        if (cl_is_raise(sid)) { json_free(exc); return sid; }
        json_t *args_arr = json_array();
        json_append(args_arr, json_copy(sid));
        json_append(args_arr, json_string(cl_raise_msg(exc)));
        json_t *lr = cl_log(io, "warning",
            "Bot Chat capability refresh kept the previous tools for session %s: %s", args_arr, 0);
        json_free(exc);
        if (cl_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
}

/* PoP: cl_bot_chat_prompt_stale @ agent/conversation_loop.py:_bot_chat_prompt_stale */
static json_t *cl_bot_chat_prompt_stale(cl_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = cl_self(st, "agent");
    const json_t *stored_prompt = cl_get(in, "stored_prompt");
    if (!cl_truth(stored_prompt)) return json_bool(false);
    /* outer try */
    json_t *home = NULL;
    {
        json_t *a = cl_args();
        cl_arg_j(a, "agent", cl_get(st, "agent"));
        json_t *r = cl_call(io, "agent_home", a);
        if (cl_is_raise(r)) {
            if (!cl_exc_is_exception(cl_kind(r))) return r; /* BaseException past both guards */
            json_free(r); home = NULL;
        } else home = r;
    }
    {
        json_t *a = cl_args();
        cl_arg_j(a, "stored_prompt", stored_prompt);
        cl_arg_j(a, "home", home);
        json_t *r = cl_call(io, "stored_prompt_capability_stale", a);
        if (cl_is_raise(r)) {
            json_free(home);
            if (!cl_exc_is_exception(cl_kind(r))) return r;
            return json_bool(false);
        }
        if (cl_truth(r)) { json_free(r); json_free(home); return json_bool(true); }
        json_free(r);
    }
    {
        json_t *bmp = cl_get(agent, "_bot_mode_protocol");
        int bmp_truth = bmp ? cl_truth(bmp) : 1; /* getattr default True */
        if (!bmp_truth) { json_free(home); return json_bool(false); }
    }
    /* title = str(getattr(agent,"_session_title_hint","") or "").strip() */
    json_t *sth = cl_get(agent, "_session_title_hint");
    char *title_raw = cl_truth(sth) ? cl_pystr(sth) : cl_xfmt("");
    char *title = cl_strip(title_raw ? title_raw : "");
    free(title_raw);
    if (!title[0]) {
        json_t *sdb = cl_req_attr(cl_get(st, "agent"), "_session_db");
        if (cl_is_raise(sdb)) { free(title); json_free(home); return sdb; }
        json_t *sid = cl_req_attr(cl_get(st, "agent"), "session_id");
        if (cl_is_raise(sid)) { free(title); json_free(home); return sid; }
        if (cl_truth(sdb) && cl_truth(sid)) {
            /* inner try: store = agent_interface(agent); title = ... */
            json_t *a = cl_args();
            cl_arg_j(a, "agent", cl_get(st, "agent"));
            json_t *store = cl_call(io, "agent_interface", a);
            json_t *ttl = NULL;
            if (cl_is_raise(store)) {
                if (!cl_exc_is_exception(cl_kind(store))) { free(title); json_free(home); return store; }
                json_free(store); store = NULL; ttl = NULL;
                goto inner_done;
            }
            {
                json_t *ta = cl_args();
                cl_arg_j(ta, "session_id", sid);
                json_t *r;
                if (!cl_is_null(store)) r = cl_call(io, "sessions_get_title", ta);
                else r = cl_call(io, "db_get_session_title", ta);
                if (cl_is_raise(r)) {
                    if (!cl_exc_is_exception(cl_kind(r))) { free(title); json_free(home); json_free(store); return r; }
                    json_free(r); ttl = NULL;
                } else ttl = r;
            }
            json_free(store);
inner_done:
            {
                char *raw = cl_truth(ttl) ? cl_pystr(ttl) : cl_xfmt("");
                char *stripped = cl_strip(raw ? raw : "");
                free(raw);
                free(title);
                title = stripped;
                json_free(ttl);
            }
        }
    }
    /* return title == BOT_CHAT_TITLE and bool(stored_bot_chat_prompt_needs_upgrade(...)) */
    int match = (strcmp(title, CL_BOT_CHAT_TITLE) == 0);
    free(title);
    if (!match) { json_free(home); return json_bool(false); }
    {
        json_t *a = cl_args();
        cl_arg_j(a, "stored_prompt", stored_prompt);
        cl_arg_j(a, "home", home);
        json_t *r = cl_call(io, "stored_bot_chat_prompt_needs_upgrade", a);
        json_free(home);
        if (cl_is_raise(r)) {
            if (!cl_exc_is_exception(cl_kind(r))) return r;
            return json_bool(false);
        }
        int need = cl_truth(r);
        json_free(r);
        return json_bool(need ? true : false);
    }
}

/* empty usage-window triple ["", null, null]. */
static json_t *cl_uw_empty(void)
{
    json_t *o = json_array();
    json_append(o, json_string(""));
    json_append(o, json_null());
    json_append(o, json_null());
    return o;
}

/* PoP: cl_usage_window_refusal @ agent/conversation_loop.py:_usage_window_refusal */
static json_t *cl_usage_window_refusal(cl_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = cl_self(st, "agent");
    const json_t *classified = cl_get(in, "classified");
    const json_t *api_messages = cl_get(in, "api_messages");
    json_t *oexc = NULL;      /* the Exception that jumps to outer_catch */
    json_t *top_tier = NULL, *stop = NULL, *resets_ms = NULL, *limit = NULL;
    char *message = NULL;
    /* context = getattr(classified,"error_context",None) or {} */
    json_t *ec = cl_dattr(classified, "error_context");
    json_t *context = cl_truth(ec) ? ec : NULL;
    if (!cl_truth(cl_get(context, "usage_window"))) return cl_uw_empty();
    /* outer try */
    json_t *state = cl_get(agent, "_credits_state");
    {
        json_t *a = cl_args();
        cl_arg_j(a, "agent", cl_get(st, "agent"));
        top_tier = cl_call(io, "current_model_is_top_tier", a);
        if (cl_is_raise(top_tier)) { oexc = top_tier; top_tier = NULL; goto outer_catch; }
    }
    if (!cl_is_null(state)) {
        json_t *a = cl_args();
        cl_arg_j(a, "state", state);
        cl_arg_b(a, "model_is_free", 0);
        cl_arg_j(a, "model_is_top_tier", top_tier);
        stop = cl_call(io, "window_hard_stop", a);
        if (cl_is_raise(stop)) { oexc = stop; stop = NULL; goto outer_catch; }
    }
    {
        json_t *display = cl_get(context, "display_message");
        json_t *cmsg = cl_dattr(classified, "message");
        char *sm_raw = cl_truth(cmsg) ? cl_pystr(cmsg) : cl_xfmt("");
        char *server_message = cl_strip(sm_raw ? sm_raw : "");
        free(sm_raw);
        const char *disp_str = cl_str(display);
        if (disp_str) {
            char *ds = cl_strip(disp_str);
            if (ds && ds[0]) message = ds;
            else free(ds);
        }
        if (!message && cl_truth(stop)) {
            const char *stop_str = cl_str(stop);
            char *rp = cl_removeprefix(stop_str ? stop_str : "", CL_XMARK " ");
            message = cl_strip(rp);
            free(rp);
        }
        if (!message && server_message[0]) message = cl_xfmt("%s", server_message);
        free(server_message);
        if (!message) { json_free(stop); json_free(top_tier); return cl_uw_empty(); }
    }
    if (!cl_is_null(state)) {
        json_t *a = cl_args();
        cl_arg_j(a, "state", state);
        cl_arg_j(a, "model_is_top_tier", top_tier);
        resets_ms = cl_call(io, "window_stop_resets_at_ms", a);
        if (cl_is_raise(resets_ms)) { oexc = resets_ms; resets_ms = NULL; goto outer_catch; }
    }
    /* inner try: limit payload */
    {
        json_t *inner_exc = NULL;
        json_t *needed = NULL;
        if (cl_truth(api_messages)) {
            json_t *a = cl_args();
            cl_arg_j(a, "arg0", api_messages);
            needed = cl_call(io, "estimate_messages_tokens_rough", a);
            if (cl_is_raise(needed)) { inner_exc = needed; needed = NULL; goto inner_done; }
        }
        {
            json_t *a = cl_args();
            cl_arg_j(a, "agent", cl_get(st, "agent"));
            cl_arg_s(a, "message", message);
            cl_arg_j(a, "needed_tokens", needed);
            json_free(needed);
            json_t *hit = cl_call(io, "limit_hit", a);
            if (cl_is_raise(hit)) { inner_exc = hit; goto inner_done; }
            if (!cl_is_null(hit)) {
                json_t *pr = cl_call(io, "limit_hit_payload", cl_args());
                if (cl_is_raise(pr)) { inner_exc = pr; json_free(hit); goto inner_done; }
                limit = pr;
            }
            json_free(hit);
        }
inner_done:
        if (inner_exc) {
            if (!cl_exc_is_exception(cl_kind(inner_exc))) {
                oexc = inner_exc; goto outer_catch; /* BaseException past inner AND outer */
            }
            json_t *lr = cl_log(io, "debug", "Usage-window limit payload unavailable", NULL, 1);
            json_free(inner_exc);
            if (cl_is_raise(lr)) { oexc = lr; goto outer_catch; }
            json_free(lr);
            json_free(limit); limit = NULL;
        }
    }
    {
        json_t *out = json_array();
        json_append(out, json_string(message));
        if (cl_is_int(resets_ms) && cl_num(resets_ms) > 0)
            json_append(out, json_number(cl_num(resets_ms) / 1000.0));
        else
            json_append(out, json_null());
        json_append(out, limit ? limit : json_null());
        free(message); json_free(stop); json_free(top_tier); json_free(resets_ms);
        return out;
    }
outer_catch:
    free(message); json_free(stop); json_free(top_tier); json_free(resets_ms); json_free(limit);
    if (oexc && !cl_exc_is_exception(cl_kind(oexc))) return oexc;
    {
        json_t *lr = cl_log(io, "debug", "Usage-window refusal parse failed", NULL, 1);
        json_free(oexc);
        if (cl_is_raise(lr)) return lr;
        json_free(lr);
        return cl_uw_empty();
    }
}

/* PoP: cl_arm_fallback_restart @ agent/conversation_loop.py:_arm_fallback_restart */
static json_t *cl_arm_fallback_restart(cl_io_t *io, const json_t *in, json_t *st)
{
    json_t *retry = cl_self(st, "retry");
    json_t *a = cl_args();
    cl_arg_j(a, "agent", cl_get(st, "agent"));
    cl_arg_j(a, "api_messages", cl_get(in, "api_messages"));
    cl_arg_j(a, "active_system_prompt", cl_get(in, "active_system_prompt"));
    json_t *asp = cl_call(io, "sync_failover_system_message", a);
    if (cl_is_raise(asp)) return asp;
    cl_dset(cl_get(st, "retry"), "primary_recovery_attempted", json_bool(false));
    cl_dset(cl_get(st, "retry"), "restart_with_rebuilt_messages", json_bool(true));
    return asp;
}

/* PoP: cl_preflight_timeout_result @ agent/conversation_loop.py:_preflight_timeout_result */
static json_t *cl_preflight_timeout_result(cl_io_t *io, const json_t *in, json_t *st)
{
    const json_t *exc = cl_get(in, "exc");
    const char *exc_msg = cl_raise_msg(exc);
    /* logger.warning(fmt, exc) — exc projected to str(exc) */
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(exc_msg));
    json_t *lr = cl_log(io, "warning",
        "Turn-start preflight compression timed out " CL_EMDASH " ending turn with typed recovery result: %s",
        args_arr, 0);
    if (cl_is_raise(lr)) return lr;
    json_free(lr);
    /* note_turn_persisted(agent) */
    json_t *na = cl_args();
    cl_arg_j(na, "agent", cl_get(st, "agent"));
    json_t *nr = cl_call(io, "note_turn_persisted", na);
    if (cl_is_raise(nr)) return nr;
    json_free(nr);
    /* return _partial_turn_result(str(exc), list(conversation_history or []), 0, ...) */
    const json_t *ch = cl_get(in, "conversation_history");
    json_t *msgs = cl_truth(ch) ? json_copy(ch) : json_array();
    json_t *flags = json_object();
    json_set(flags, "failed", json_bool(true));
    json_set(flags, "compression_exhausted", json_bool(true));
    json_set(flags, "turn_exit_reason", json_string("context_compression_timeout"));
    json_set(flags, "failure_reason", json_string("context_overflow"));
    json_set(flags, "failure_retryable", json_bool(false));
    json_t *pin = json_object();
    json_set(pin, "final_response", json_string(exc_msg));
    json_set(pin, "messages", msgs);
    json_set(pin, "api_call_count", json_number(0));
    json_set(pin, "flags", flags);
    return cl_partial_turn_result(io, pin, st);
}

/* ── _run_phase core: call a phase helper double with the loop locals it names,
 *    copy its verdict fields back into the state double. fn is a callable double
 *    carrying __callable__ (seam), __params__ (signature minus agent) and
 *    __name__ (for the latched-field rule). extra supplies non-state arguments. ── */
static json_t *cl_run_phase_core(cl_io_t *io, const json_t *fn, const json_t *agent,
                                 json_t *sw, const json_t *extra)
{
    const json_t *params = cl_get(fn, "__params__");
    const char *seam = cl_str(cl_get(fn, "__callable__"));
    const char *fname = cl_str(cl_get(fn, "__name__"));
    /* kwargs = {n: extra[n] if n in extra else getattr(state, n) for n in params} */
    json_t *kwargs = json_object();
    if (cl_is_arr(params)) {
        for (size_t i = 0; i < json_len(params); i++) {
            const char *n = cl_str(json_get(params, i));
            if (!n) continue;
            json_t *v = NULL;
            if (cl_is_obj(extra) && json_has(extra, n)) {
                json_t *ev = cl_get(extra, n);
                if (cl_is_raise(ev)) v = json_string(cl_raise_msg(ev));
                else v = cl_copy_or_null(ev);
            } else {
                json_t *gv = cl_req_attr(sw, n);
                if (cl_is_raise(gv)) { json_free(kwargs); return gv; }
                v = json_copy(gv);
            }
            json_set(kwargs, n, v);
        }
    }
    /* verdict = fn(agent, **kwargs) */
    json_t *a = cl_args();
    cl_arg_j(a, "agent", agent);
    for (size_t i = 0; i < kwargs->c.count; i++)
        json_set(a, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
    json_free(kwargs);
    json_t *verdict = cl_call(io, seam ? seam : "phase", a);
    if (cl_is_raise(verdict)) return verdict;
    /* latched = _LATCHED_VERDICT_FIELDS.get(fn.__name__, ()) */
    int latched = (fname && strcmp(fname, "handle_api_error") == 0);
    if (cl_is_obj(verdict)) {
        for (size_t i = 0; i < verdict->c.count; i++) {
            const char *f = verdict->c.keys[i];
            if (strcmp(f, "action") == 0 || strcmp(f, "result") == 0) continue;
            json_t *value = verdict->c.items[i];
            int is_latched_field = latched && strcmp(f, "_provider_overflow_recovery_pending") == 0;
            if (!is_latched_field) {
                cl_dset(sw, f, json_copy(value));
            } else if (cl_truth(value)) {
                cl_dset(sw, f, json_bool(true));
            }
        }
    }
    return verdict;
}

/* PoP: cl_run_phase @ agent/conversation_loop.py:_run_phase */
static json_t *cl_run_phase(cl_io_t *io, const json_t *in, json_t *st)
{
    cl_self(st, "state"); /* vivify the state double wrapper */
    json_t *sw = cl_get(st, "state");
    return cl_run_phase_core(io, cl_get(in, "fn"), cl_get(in, "agent"), sw, cl_get(in, "extra"));
}

/* PoP: cl_run_api_retry_loop @ agent/conversation_loop.py:_run_api_retry_loop */
static json_t *cl_run_api_retry_loop(cl_io_t *io, const json_t *in, json_t *st)
{
    cl_self(st, "s");
    json_t *sw = cl_get(st, "s");
    const json_t *agent = cl_get(st, "agent");
    const json_t *phases = cl_get(in, "phases");
    for (;;) {
        double rc = cl_num(cl_dattr(sw, "retry_count"));
        double mr = cl_num(cl_dattr(sw, "max_retries"));
        if (!(rc < mr)) break;
        /* _ng = _run_phase(nous_rate_limit_guard, agent, s) — outside the try */
        json_t *ng = cl_run_phase_core(io, cl_get(phases, "nous_rate_limit_guard"), agent, sw, NULL);
        if (cl_is_raise(ng)) return ng;
        const char *ng_action = cl_str(cl_get(ng, "action"));
        if (ng_action && strcmp(ng_action, "return") == 0) {
            json_t *r = cl_copy_or_null(cl_get(ng, "result")); json_free(ng); return r;
        }
        if (ng_action && strcmp(ng_action, "break") == 0) { json_free(ng); return NULL; }
        json_free(ng);
        /* try: build -> perform -> check */
        json_t *exc = NULL;
        json_t *b = cl_run_phase_core(io, cl_get(phases, "build_api_request"), agent, sw, NULL);
        if (cl_is_raise(b)) { exc = b; goto handled; }
        json_free(b);
        json_t *p = cl_run_phase_core(io, cl_get(phases, "perform_api_call"), agent, sw, NULL);
        if (cl_is_raise(p)) { exc = p; goto handled; }
        {
            const char *pa = cl_str(cl_get(p, "action"));
            if (pa && strcmp(pa, "break") == 0) { json_free(p); return NULL; }
        }
        json_free(p);
        json_t *ck = cl_run_phase_core(io, cl_get(phases, "check_api_response"), agent, sw, NULL);
        if (cl_is_raise(ck)) { exc = ck; goto handled; }
        {
            const char *ca = cl_str(cl_get(ck, "action"));
            if (ca && strcmp(ca, "return") == 0) {
                json_t *r = cl_copy_or_null(cl_get(ck, "result")); json_free(ck); return r;
            }
            if (ca && strcmp(ca, "break") == 0) { json_free(ck); return NULL; }
        }
        json_free(ck);
        continue; /* success path: loop re-checks retry_count < max_retries */
handled:
        /* except InterruptedError / except Exception */
        if (cl_exc_is_instance(cl_kind(exc), "InterruptedError")) {
            json_t *hi = cl_run_phase_core(io, cl_get(phases, "handle_api_interrupt"), agent, sw, NULL);
            json_free(exc);
            if (cl_is_raise(hi)) return hi;
            const char *ha = cl_str(cl_get(hi, "action"));
            if (ha && strcmp(ha, "break") == 0) { json_free(hi); return NULL; }
            json_free(hi);
            continue;
        }
        if (cl_exc_is_exception(cl_kind(exc))) {
            json_t *extra = json_object();
            json_set(extra, "api_error", exc); /* ownership transferred */
            json_t *ae = cl_run_phase_core(io, cl_get(phases, "handle_api_error"), agent, sw, extra);
            json_free(extra);
            if (cl_is_raise(ae)) return ae;
            const char *aa = cl_str(cl_get(ae, "action"));
            if (aa && strcmp(aa, "return") == 0) {
                json_t *r = cl_copy_or_null(cl_get(ae, "result")); json_free(ae); return r;
            }
            if (aa && strcmp(aa, "break") == 0) { json_free(ae); return NULL; }
            json_free(ae);
            continue;
        }
        return exc; /* BaseException passthrough */
    }
    return NULL;
}

/* PoP: cl_finalize_from_state @ agent/conversation_loop.py:_finalize_from_state */
static json_t *cl_finalize_from_state(cl_io_t *io, const json_t *in, json_t *st)
{
    cl_self(st, "s");
    json_t *sw = cl_get(st, "s");
    const json_t *agent = cl_get(st, "agent");
    const json_t *params = cl_get(in, "finalize_params");
    json_t *a = cl_args();
    cl_arg_j(a, "agent", agent);
    if (cl_is_arr(params)) {
        for (size_t i = 0; i < json_len(params); i++) {
            const char *n = cl_str(json_get(params, i));
            if (!n) continue;
            json_t *v = cl_req_attr(sw, n);
            if (cl_is_raise(v)) { json_free(a); return v; }
            json_set(a, n, json_copy(v));
        }
    }
    return cl_call(io, "finalize_turn", a);
}

/* forward decl for the composed sibling */
static json_t *cl_codex_app_server_turn(cl_io_t *io, const json_t *in, json_t *st);

/* PoP: cl_turn_without_loop @ agent/conversation_loop.py:_turn_without_loop */
static json_t *cl_turn_without_loop(cl_io_t *io, const json_t *in, json_t *st)
{
    cl_self(st, "s");
    json_t *sw = cl_get(st, "s");
    const json_t *block_message = cl_get(in, "block_message");
    if (cl_truth(block_message)) {
        cl_dset(sw, "final_response", json_copy(block_message));
        cl_dset(sw, "_turn_exit_reason", json_string(CL_PRE_LLM_BLOCK_EXIT_REASON));
        return cl_finalize_from_state(io, in, st);
    }
    json_t *agent = cl_self(st, "agent");
    json_t *api_mode = cl_req_attr(cl_get(st, "agent"), "api_mode");
    if (cl_is_raise(api_mode)) return api_mode;
    const char *am = cl_str(api_mode);
    if (am && strcmp(am, "codex_app_server") == 0)
        return cl_codex_app_server_turn(io, in, st);
    return NULL;
}

/* PoP: cl_codex_app_server_turn @ agent/conversation_loop.py:_codex_app_server_turn */
static json_t *cl_codex_app_server_turn(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    cl_self(st, "s");
    json_t *sw = cl_get(st, "s");
    json_t *agent = cl_self(st, "agent");
    /* codex_result = agent._run_codex_app_server_turn(user_message=s.user_message, ...) */
    json_t *a = cl_args();
    static const char *fields[] = {
        "user_message", "original_user_message", "messages",
        "effective_task_id", "_should_review_memory", "turn_id", NULL,
    };
    static const char *kwname[] = {
        "user_message", "original_user_message", "messages",
        "effective_task_id", "should_review_memory", "turn_id", NULL,
    };
    for (size_t i = 0; fields[i]; i++) {
        json_t *v = cl_req_attr(sw, fields[i]);
        if (cl_is_raise(v)) { json_free(a); return v; }
        json_set(a, kwname[i], json_copy(v));
    }
    json_t *codex_result = cl_call(io, "run_codex_app_server_turn", a);
    if (cl_is_raise(codex_result)) return codex_result;
    /* if not activate_codex_app_server_fallback(agent, codex_result): return codex_result */
    json_t *fa = cl_args();
    cl_arg_j(fa, "agent", cl_get(st, "agent"));
    cl_arg_j(fa, "codex_result", codex_result);
    json_t *activated = cl_call(io, "activate_codex_app_server_fallback", fa);
    if (cl_is_raise(activated)) { json_free(codex_result); return activated; }
    if (!cl_truth(activated)) { json_free(activated); return codex_result; }
    json_free(activated);
    /* s.api_call_count = int(codex_result.get("api_calls") or 0) */
    json_t *ac = cl_get(codex_result, "api_calls");
    json_t *orv = cl_truth(ac) ? ac : NULL;
    json_t *zero = json_number(0);
    json_t *as_int = cl_pyint(orv ? orv : zero);
    json_free(zero);
    if (cl_is_raise(as_int)) { json_free(codex_result); return as_int; }
    cl_dset(sw, "api_call_count", as_int);
    /* s.active_system_prompt = _sync_failover_system_message(agent, None, s.active_system_prompt) */
    json_t *cur = cl_req_attr(sw, "active_system_prompt");
    if (cl_is_raise(cur)) { json_free(codex_result); return cur; }
    json_t *sa = cl_args();
    cl_arg_j(sa, "agent", cl_get(st, "agent"));
    /* api_messages=None -> dropped */
    cl_arg_j(sa, "active_system_prompt", cur);
    json_t *newasp = cl_call(io, "sync_failover_system_message", sa);
    json_free(codex_result);
    if (cl_is_raise(newasp)) return newasp;
    cl_dset(sw, "active_system_prompt", newasp);
    return NULL;
}

/* `x is True` identity gate: JSON_BOOL true only. */
static int cl_is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }

/* PoP: cl_failed_turn_display_metadata @ agent/conversation_loop.py:_failed_turn_display_metadata */
static json_t *cl_failed_turn_display_metadata(cl_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = cl_self(st, "agent");
    const json_t *result = cl_get(in, "result");
    json_t *surface = NULL;
    /* try: surface = build_error_surface_from_result(result, provider=agent.provider or "", model=...) */
    json_t *prov = cl_req_attr(cl_get(st, "agent"), "provider");
    if (cl_is_raise(prov)) {
        if (!cl_exc_is_exception(cl_kind(prov))) return prov;
        goto surf_except;
    }
    json_t *model = cl_req_attr(cl_get(st, "agent"), "model");
    if (cl_is_raise(model)) {
        if (!cl_exc_is_exception(cl_kind(model))) return model;
        goto surf_except;
    }
    {
        json_t *a = cl_args();
        cl_arg_j(a, "result", result);
        json_set(a, "provider", cl_truth(prov) ? json_copy(prov) : json_string(""));
        json_set(a, "model", cl_truth(model) ? json_copy(model) : json_string(""));
        surface = cl_call(io, "build_error_surface_from_result", a);
        if (cl_is_raise(surface)) {
            if (!cl_exc_is_exception(cl_kind(surface))) return surface;
            json_free(surface); surface = NULL;
            goto surf_except;
        }
    }
    goto after_surf;
surf_except:
    {
        json_t *lr = cl_log(io, "debug", "failed-turn error surface unavailable", NULL, 1);
        if (cl_is_raise(lr)) return lr;
        json_free(lr);
        surface = NULL;
    }
after_surf:
    {
        /* error = str(result.get("error") or "").strip()[:2000] */
        json_t *err_v = cl_get(result, "error");
        char *err_raw = cl_truth(err_v) ? cl_pystr(err_v) : cl_xfmt("");
        char *err_strip = cl_strip(err_raw ? err_raw : "");
        free(err_raw);
        size_t elen = strlen(err_strip);
        if (elen > CL_FAILED_TURN_ERROR_MAX_CHARS) elen = CL_FAILED_TURN_ERROR_MAX_CHARS;
        json_t *error = cl_strn(err_strip, elen);
        free(err_strip);
        /* metadata = {k:v for (error,error),(error_surface,surface) if v} */
        json_t *md = json_object();
        if (cl_truth(error)) json_set(md, "error", json_copy(error));
        if (cl_truth(surface)) json_set(md, "error_surface", json_copy(surface));
        json_free(error); json_free(surface);
        json_t *pin = json_object();
        json_set(pin, "metadata", md);
        return cl_redact_display_metadata(io, pin, st);
    }
}

/* PoP: cl_close_durable_failed_turn @ agent/conversation_loop.py:_close_durable_failed_turn */
static json_t *cl_close_durable_failed_turn(cl_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *agent = cl_self(st, "agent");
    json_t *result = cl_get(st, "result");
    /* try: */
    json_t *exc = NULL;
    if (!cl_is_plain_dict(result) || cl_is_true(cl_get(result, "completed"))) return NULL;
    {
        const char *fr = cl_str(cl_get(result, "failure_reason"));
        if (cl_truth(cl_get(result, "compression_exhausted"))
            || cl_truth(cl_get(result, "compression_deferred"))
            || (fr && (strcmp(fr, "context_overflow") == 0 || strcmp(fr, "content_policy_blocked") == 0)))
            return NULL;
    }
    json_t *messages = cl_get(result, "messages");
    json_t *db = cl_get(agent, "_session_db");
    json_t *session_id = cl_get(agent, "session_id");
    if (!cl_is_arr(messages) || json_len(messages) == 0 || cl_is_null(db) || !cl_truth(session_id))
        return NULL;
    {
        json_t *a = cl_args();
        cl_arg_j(a, "agent", cl_get(st, "agent"));
        json_t *store = cl_call(io, "agent_interface", a);
        if (cl_is_raise(store)) { exc = store; goto handle; }
        if (cl_truth(cl_get(agent, "_persist_disabled"))) { json_free(store); return NULL; }
        /* (store.messages if store is not None else db).latest_conversation_role(session_id) */
        if (!cl_is_null(store)) {
            json_t *sm = cl_req_attr(store, "messages");
            if (cl_is_raise(sm)) { exc = sm; json_free(store); goto handle; }
        }
        json_t *la = cl_args();
        cl_arg_j(la, "session_id", session_id);
        json_t *role = cl_call(io, "latest_conversation_role", la);
        json_free(store);
        if (cl_is_raise(role)) { exc = role; goto handle; }
        const char *rs = cl_str(role);
        int open = (rs && (strcmp(rs, "user") == 0 || strcmp(rs, "tool") == 0));
        json_free(role);
        if (!open) return NULL;
    }
    {
        /* start = result.get("current_turn_user_idx"); turn_messages = messages[start:] if valid else messages */
        json_t *start = cl_get(result, "current_turn_user_idx");
        json_t *turn_messages = messages;
        json_t *sliced = NULL;
        if (cl_is_int(start)) {
            double s = cl_num(start);
            if (s >= 0 && s < (double)json_len(messages)) {
                sliced = json_array();
                for (size_t i = (size_t)s; i < json_len(messages); i++)
                    json_append(sliced, json_copy(json_get(messages, i)));
                turn_messages = sliced;
            }
        }
        /* boundary = {"role":"assistant","content":failed_turn_notice(turn_messages),"display_kind":"failed_turn"} */
        json_t *fa = cl_args();
        cl_arg_j(fa, "turn_messages", turn_messages);
        json_t *notice = cl_call(io, "failed_turn_notice", fa);
        json_free(sliced);
        if (cl_is_raise(notice)) { exc = notice; goto handle; }
        json_t *boundary = json_object();
        json_set(boundary, "role", json_string("assistant"));
        json_set(boundary, "content", notice);
        json_set(boundary, "display_kind", json_string("failed_turn"));
        /* if failure := _failed_turn_display_metadata(agent, result): boundary["display_metadata"]=failure */
        json_t *pin = json_object();
        json_set(pin, "result", json_copy(result));
        json_t *failure = cl_failed_turn_display_metadata(io, pin, st);
        json_free(pin);
        if (cl_is_raise(failure)) { exc = failure; json_free(boundary); goto handle; }
        if (cl_truth(failure)) json_set(boundary, "display_metadata", failure);
        else json_free(failure);
        /* append_message(messages, boundary): stamp timestamp (wall_time) + append */
        json_t *aa = cl_args();
        cl_arg_j(aa, "message", boundary);
        json_t *ar = cl_call(io, "append_message", aa);
        if (cl_is_raise(ar)) { exc = ar; json_free(boundary); goto handle; }
        json_free(ar);
        if (cl_is_null(cl_get(boundary, "timestamp"))) {
            json_t *wt = cl_call(io, "wall_time", cl_args());
            if (cl_is_raise(wt)) { exc = wt; json_free(boundary); goto handle; }
            json_set(boundary, "timestamp", wt);
        }
        json_append(messages, boundary);
        /* agent._flush_messages_to_session_db(messages) */
        json_t *fla = cl_args();
        cl_arg_j(fla, "arg0", messages);
        json_t *flr = cl_call(io, "flush_messages_to_session_db", fla);
        if (cl_is_raise(flr)) { exc = flr; goto handle; }
        json_free(flr);
    }
    return NULL;
handle:
    if (!cl_exc_is_exception(cl_kind(exc))) return exc;
    {
        json_t *lr = cl_log(io, "debug", "failed-turn boundary not written", NULL, 1);
        json_free(exc);
        if (cl_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
}
