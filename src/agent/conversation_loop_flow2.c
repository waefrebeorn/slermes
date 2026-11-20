/* conversation_loop_flow2.c — second-wave FLOW port of agent/conversation_loop.py
 * (LIVE authority). Ports the UNCLAIMED remainder: the review-fork input-budget
 * gate (_review_input_budget_exhausted), the completed-compaction rearm probe
 * (_should_rearm_compression_budget), the billing terminal prefix
 * (_billing_terminal_label), the structured billing terminal result
 * (_billing_failure_result), the per-turn loop-state dataclass construction
 * (_LoopState), and the 161-line outer turn driver (_run_conversation_turn).
 *
 * Claimed siblings (_decode_inline_moa_turn, _preflight_timeout_result,
 * _turn_without_loop, _run_phase, _run_api_retry_loop, _finalize_from_state,
 * _billing_or_entitlement_message, _billing_block_dict, _restore_or_build_system_prompt,
 * _ra) and all foreign I/O enter through the cl2_io_t universal seam; ported
 * siblings compose for REAL (_billing_terminal_label inside _billing_failure_result,
 * _LoopState inside _run_conversation_turn). The seamed _run_phase is modeled by its
 * contract at the call site: the scripted verdict is copied back onto the loop-state
 * double (skip action/result; the handle_api_error latch only ever sets True).
 *
 * Decision sequences only: Python truthiness, `or`/`and` short-circuits, `is None`
 * identity vs truthy tests, kwarg/operand evaluation order, dict insertion order,
 * chained-assignment targets, and except-Exception / except-PreflightCompressionTimedOut
 * catch scopes are transcribed per call site. A scripted BaseException (SystemExit /
 * KeyboardInterrupt / GeneratorExit) propagates past every except Exception guard
 * exactly as live (full builtin exception-parent table below).
 *
 * Value model (shared with tests/oracle_conversation_loop_flow2.py):
 *   - attribute doubles {"__attr__": {...}} <-> SimpleNamespace; the state window
 *     keeps the wrapper form and is mutated in place.
 *   - class-tagged doubles {"__class__": "_LoopState"} give type() identity.
 *   - callable doubles {"__callable__": "<seam>"} (callable() is True).
 *   - raise specs {"__raise__": "<ClassName>", "message": str(e)}.
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - _review_input_budget_exhausted isinstance(int) gates: integral numbers must be
 *     integral-INT in fixtures (an integral-float reads as int in libjson but is a
 *     float live); non-int tests use bool / str / non-integral float.
 *   - _billing_failure_result usage_window travels as a JSON array (tuple() of a list);
 *     a present-null usage_window raises TypeError "'NoneType' object is not iterable".
 *   - _run_conversation_turn keeps every _LoopState param field present on the state
 *     double (the seamed _run_phase verdict copyback never reads an absent field); the
 *     loop fixtures stay in the terminating domain (scripted break/return verdicts).
 *   - set() state writes (_delivered_interim_texts) project to a sorted array (empty).
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified "agent/conversation_loop_flow2.c".
 * Every symbol is static cl2_*.
 *
 * Gate: bash tests/run_one_oracle.sh conversation_loop_flow2 \
 *           oracle_conversation_loop_flow2.py "" "lib/libjson/json.o"
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

/* ── live module constants (byte-pinned to agent/conversation_loop.py) ── */
/* U+2014 em dash (hex-escaped). */
#define CL2_EMDASH "\xe2\x80\x94"
#define CL2_PRE_LLM_BLOCK_EXIT_REASON "blocked_by_plugin_pre_llm_call"
/* _COMPRESSION_TIMEOUT_FINAL_RESPONSE (agent/conversation_loop.py). */
#define CL2_COMPRESSION_TIMEOUT_FINAL_RESPONSE \
    "Context compression timed out without reducing this conversation. No messages were " \
    "dropped. Start a fresh session with /new, or check auxiliary.compression before retrying /compress."

/* ── universal seam ── */
typedef struct cl2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cl2_io_t;

/* ── truthiness / accessors ── */
static int cl2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cl2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cl2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cl2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cl2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cl2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cl2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cl2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cl2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cl2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static json_t *cl2_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* isinstance(x, int): a number OR a bool (bool is an int subclass). Integral
 * floats read as int in libjson; the fixture domain keeps integral numbers int. */
static int cl2_is_int(const json_t *v)
{
    if (cl2_is_bool(v)) return 1;
    if (!cl2_is_num(v)) return 0;
    double d = v->num_val;
    return isfinite(d) && d == (double)(long long)d;
}

static const char *cl2_class(const json_t *v) { return cl2_str(cl2_get(v, "__class__")); }

static const char *cl2_typename(const json_t *v)
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
            const char *tag = cl2_class(v);
            if (tag) return tag;
            return json_obj_get(v, "__attr__") ? "types.SimpleNamespace" : "dict";
        }
        default: return "object";
    }
}

/* ── state-window helpers ── */
static json_t *cl2_self(json_t *st, const char *key)
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

static json_t *cl2_dattr(const json_t *d, const char *name)
{
    return cl2_get(cl2_get(d, "__attr__"), name);
}

static void cl2_dset(const json_t *d, const char *name, json_t *val)
{
    json_t *inner = cl2_get(d, "__attr__");
    if (inner && inner->type == JSON_OBJECT) json_set(inner, name, val);
    else json_free(val);
}

/* ── raise specs ── */
static int cl2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cl2_kind(const json_t *r)
{
    const char *k = cl2_str(cl2_get(r, "__raise__"));
    return k ? k : "";
}

static const char *cl2_raise_msg(const json_t *r)
{
    const char *m = cl2_str(cl2_get(r, "message"));
    return m ? m : "";
}

static json_t *cl2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *cl2_xfmt(const char *fmt, ...)
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

/* ── FULL builtin exception-parent hierarchy (py3.14). BaseException trio (and
 *    asyncio.CancelledError) parent to BaseException ONLY, so they pass through
 *    every except Exception. PreflightCompressionTimedOut is a registered
 *    Exception subclass (agent.turn_context). ── */
typedef struct { const char *name; const char *parent; } cl2_exc_row;
static const cl2_exc_row CL2_EXC_PARENTS[] = {
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
    {"PreflightCompressionTimedOut", "Exception"},
    {NULL, NULL},
};

static const char *cl2_exc_parent(const char *name)
{
    for (size_t i = 0; CL2_EXC_PARENTS[i].name; i++)
        if (strcmp(CL2_EXC_PARENTS[i].name, name) == 0) return CL2_EXC_PARENTS[i].parent;
    /* unknown synthetic kind: a direct Exception subclass (oracle type(kind,(Exception,))). */
    return "Exception";
}

static int cl2_exc_is_instance(const char *kind, const char *base)
{
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, base) == 0) return 1;
        const char *p = cl2_exc_parent(cur);
        if (!p || strcmp(cur, p) == 0) break;
        snprintf(cur, sizeof(cur), "%s", p);
        if (strcmp(cur, "BaseException") == 0) return strcmp(base, "BaseException") == 0;
    }
    return 0;
}

static int cl2_exc_is_exception(const char *kind)
{
    return cl2_exc_is_instance(kind, "Exception");
}

/* ── universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. ── */
static json_t *cl2_call(cl2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cl2_xfmt("no seam installed for %s", name);
        r = cl2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *cl2_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides. */
static void cl2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void cl2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void cl2_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void cl2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* Python str() projection (domain: str / integral number / bool / None). */
static char *cl2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cl2_xfmt("None");
    if (v->type == JSON_BOOL) return cl2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d) return cl2_xfmt("%.0f", d);
        return cl2_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cl2_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : cl2_xfmt("");
}

/* ── required attribute read: getattr(obj, name) with NO default raises
 *    AttributeError when absent. Returns the value (borrowed), or a raise spec. ── */
static json_t *cl2_req_attr(const json_t *d, const char *name)
{
    json_t *inner = cl2_get(d, "__attr__");
    json_t *v = cl2_get(inner, name);
    if (!v) {
        char *m = cl2_xfmt("'%s' object has no attribute '%s'", cl2_typename(d), name);
        json_t *r = cl2_mkraise("AttributeError", m ? m : "");
        free(m);
        return r;
    }
    return v; /* borrowed */
}

/* getattr(obj, name, default): absent -> a COPY of the default (caller owns). */
static json_t *cl2_getattr_def(const json_t *d, const char *name, const json_t *def)
{
    json_t *v = cl2_dattr(d, name);
    if (!v) return def ? json_copy(def) : json_null();
    return json_copy(v);
}

/* ── logger seam helper. ── */
static json_t *cl2_log(cl2_io_t *io, const char *level, const char *msg, json_t *args_arr, int exc_info)
{
    json_t *a = cl2_args();
    cl2_arg_s(a, "msg", msg);
    if (args_arr) json_set(a, "args", args_arr); else json_set(a, "args", json_array());
    if (exc_info) cl2_arg_b(a, "exc_info", 1);
    char *nm = cl2_xfmt("logger.%s", level);
    json_t *r = cl2_call(io, nm ? nm : "logger.debug", a);
    free(nm);
    return r;
}

/* `x is True` identity gate: JSON_BOOL true only. */
static int cl2_is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }

/* ════════════════════════ ported functions ════════════════════════ */

/* PoP: cl2_review_input_budget_exhausted @ agent/conversation_loop.py:_review_input_budget_exhausted */
static json_t *cl2_review_input_budget_exhausted(cl2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    json_t *agent = cl2_self(st, "agent");
    /* budget = getattr(agent, "_review_input_token_budget", None) */
    json_t *budget = cl2_dattr(cl2_get(st, "agent"), "_review_input_token_budget");
    /* if not isinstance(budget,int) or isinstance(budget,bool) or budget<=0: return False */
    if (!cl2_is_int(budget) || cl2_is_bool(budget) || cl2_num(budget) <= 0.0)
        return json_bool(false);
    /* used = getattr(agent, "session_input_tokens", 0) */
    json_t *used = cl2_dattr(cl2_get(st, "agent"), "session_input_tokens");
    /* return isinstance(used,int) and not isinstance(used,bool) and used>=budget */
    int r = cl2_is_int(used) && !cl2_is_bool(used) && cl2_num(used) >= cl2_num(budget);
    return json_bool(r ? true : false);
}

/* PoP: cl2_should_rearm_compression_budget @ agent/conversation_loop.py:_should_rearm_compression_budget */
static json_t *cl2_should_rearm_compression_budget(cl2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *ca = cl2_get(in, "compression_attempts");
    const json_t *ccp = cl2_get(in, "completed_compaction_pending");
    const json_t *pt = cl2_get(in, "prompt_tokens");
    const json_t *tt = cl2_get(in, "threshold_tokens");
    /* bool(compression_attempts and completed_compaction_pending and 0<prompt_tokens<threshold_tokens) */
    int r = cl2_truth(ca) && cl2_truth(ccp)
            && (0.0 < cl2_num(pt)) && (cl2_num(pt) < cl2_num(tt));
    return json_bool(r ? true : false);
}

/* PoP: cl2_billing_terminal_label @ agent/conversation_loop.py:_billing_terminal_label */
static json_t *cl2_billing_terminal_label(cl2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *summary = cl2_get(in, "summary");
    const json_t *unverified = cl2_get(in, "unverified");
    char *sum = cl2_pystr(summary);
    char *out;
    if (cl2_truth(unverified)) {
        out = cl2_xfmt("Provider reported usage/credit exhaustion (unverified " CL2_EMDASH
                       " the same error can be a content-filter rejection, not billing): %s",
                       sum ? sum : "");
    } else {
        out = cl2_xfmt("Billing or credits exhausted: %s", sum ? sum : "");
    }
    free(sum);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* forward decl: composed sibling */
static json_t *cl2_billing_terminal_label_core(cl2_io_t *io, const json_t *summary, json_t *unverified);

/* _billing_terminal_label as a value helper (summary borrowed, unverified owned). */
static json_t *cl2_billing_terminal_label_core(cl2_io_t *io, const json_t *summary, json_t *unverified)
{
    json_t *pin = json_object();
    cl2_arg_j(pin, "summary", summary);
    json_set(pin, "unverified", unverified); /* ownership of the fresh bool node -> pin */
    json_t *r = cl2_billing_terminal_label(io, pin, NULL);
    json_free(pin);
    return r;
}

/* PoP: cl2_billing_failure_result @ agent/conversation_loop.py:_billing_failure_result */
static json_t *cl2_billing_failure_result(cl2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *classified = cl2_get(in, "classified");
    const json_t *summary = cl2_get(in, "summary");
    const json_t *messages = cl2_get(in, "messages");
    const json_t *api_call_count = cl2_get(in, "api_call_count");
    const json_t *provider = cl2_get(in, "provider");
    const json_t *base_url = cl2_get(in, "base_url");
    const json_t *model = cl2_get(in, "model");
    const json_t *guidance_in = cl2_get(in, "guidance");
    const json_t *uw = cl2_get(in, "usage_window");

    /* unverified = bool(getattr(classified, "billing_unverified", False)) */
    json_t *bu = cl2_dattr(classified, "billing_unverified");
    int unverified = cl2_truth(bu);

    /* tuple(usage_window): present-non-array raises TypeError (not iterable).
     * absent -> default ("", None, None). Materialize one array to borrow from. */
    if (uw && uw->type != JSON_ARRAY) {
        char *m = cl2_xfmt("'%s' object is not iterable", cl2_typename(uw));
        json_t *r = cl2_mkraise("TypeError", m ? m : "");
        free(m);
        return r;
    }
    json_t *uw_default = NULL;
    const json_t *uw_arr = uw;
    if (!uw) {
        uw_default = json_array();
        json_append(uw_default, json_string(""));
        json_append(uw_default, json_null());
        json_append(uw_default, json_null());
        uw_arr = uw_default;
    }
    size_t uwn = json_len(uw_arr);
    const json_t *uw0 = uwn > 0 ? json_get(uw_arr, 0) : NULL;
    const json_t *uw1 = uwn > 1 ? json_get(uw_arr, 1) : NULL;
    const json_t *uw2 = uwn > 2 ? json_get(uw_arr, 2) : NULL;

    /* guidance-fetch gate: (tuple(uw)+("",))[0] -> uw[0] if len>=1 else "" (falsy). */
    int gate_empty = (uwn == 0) ? 1 : !cl2_truth(uw0);

    /* guidance = guidance_in; if guidance is None and not gate_first: fetch. */
    json_t *guidance = cl2_copy_or_null(guidance_in); /* NULL when absent/None */
    if (cl2_is_null(guidance) && gate_empty) {
        if (guidance) { json_free(guidance); guidance = NULL; }
        char *bu_str = cl2_pystr(base_url);
        json_t *a = cl2_args();
        cl2_arg_s(a, "capability", "model access");
        cl2_arg_j(a, "provider", provider);
        cl2_arg_s(a, "base_url", bu_str ? bu_str : "None");
        cl2_arg_j(a, "model", model);
        cl2_arg_b(a, "unverified", unverified);
        free(bu_str);
        json_t *g = cl2_call(io, "billing_or_entitlement_message", a);
        if (cl2_is_raise(g)) { json_free(guidance); json_free(uw_default); return g; }
        guidance = g; /* caller-owned; NULL == None */
    }

    /* unpack: (tuple(uw)+(None,None,None))[:3] -> uw[i] if len>i else None. */
    const json_t *uwm = uw0;      /* NULL when len==0 */
    const json_t *reset_at = uw1;
    const json_t *limit = uw2;

    /* final = usage_window_message or (label + (f"\n\n{guidance}" if guidance else "")) */
    json_t *final = NULL;
    if (cl2_truth(uwm)) {
        final = json_copy(uwm);
    } else {
        json_t *label = cl2_billing_terminal_label_core(io, summary, json_bool(unverified));
        if (cl2_is_raise(label)) { json_free(guidance); json_free(uw_default); return label; }
        char *lab = cl2_pystr(label);
        json_free(label);
        char *fin;
        if (cl2_truth(guidance)) {
            char *gs = cl2_pystr(guidance);
            fin = cl2_xfmt("%s\n\n%s", lab ? lab : "", gs ? gs : "");
            free(gs);
        } else {
            fin = cl2_xfmt("%s", lab ? lab : "");
        }
        free(lab);
        final = json_string(fin ? fin : "");
        free(fin);
    }

    /* extra dict (insertion order) */
    json_t *extra = json_object();
    if (cl2_truth(uwm)) {
        json_set(extra, "usage_window_message", json_copy(uwm));
        if (!cl2_is_null(reset_at)) json_set(extra, "failure_resets_at", json_copy(reset_at));
        if (cl2_truth(limit)) json_set(extra, "usage_limit", json_copy(limit));
    }

    /* failure_reason = classified.reason.value ; failure_retryable = bool(classified.retryable) */
    json_t *reason = cl2_req_attr(classified, "reason");
    if (cl2_is_raise(reason)) { json_free(extra); json_free(final); json_free(guidance); json_free(uw_default); return reason; }
    json_t *reason_val = cl2_req_attr(reason, "value");
    if (cl2_is_raise(reason_val)) { json_free(extra); json_free(final); json_free(guidance); json_free(uw_default); return reason_val; }
    json_t *retryable = cl2_req_attr(classified, "retryable");
    if (cl2_is_raise(retryable)) { json_free(extra); json_free(final); json_free(guidance); json_free(uw_default); return retryable; }

    /* billing_block = _billing_block_dict(provider, base_url, model, guidance, unverified=unverified) */
    json_t *ba = cl2_args();
    cl2_arg_j(ba, "arg0", provider);
    cl2_arg_j(ba, "arg1", base_url);
    cl2_arg_j(ba, "arg2", model);
    cl2_arg_j(ba, "arg3", guidance);
    cl2_arg_b(ba, "unverified", unverified);
    json_t *block = cl2_call(io, "billing_block_dict", ba);
    if (cl2_is_raise(block)) { json_free(extra); json_free(final); json_free(guidance); json_free(uw_default); return block; }

    /* return {**extra, final_response, messages, api_calls, completed, failed, error,
     *         failure_reason, failure_retryable, billing_unverified, billing_block} */
    json_t *out = json_object();
    for (size_t i = 0; i < extra->c.count; i++)
        json_set(out, extra->c.keys[i], json_copy(extra->c.items[i]));
    json_set(out, "final_response", final);
    json_set(out, "messages", messages ? json_copy(messages) : json_null());
    json_set(out, "api_calls", api_call_count ? json_copy(api_call_count) : json_number(0));
    json_set(out, "completed", json_bool(false));
    json_set(out, "failed", json_bool(true));
    json_set(out, "error", summary ? json_copy(summary) : json_null());
    json_set(out, "failure_reason", json_copy(reason_val));
    json_set(out, "failure_retryable", json_bool(cl2_truth(retryable) ? true : false));
    json_set(out, "billing_unverified", json_bool(unverified ? true : false));
    json_set(out, "billing_block", block ? block : json_null());
    json_free(extra);
    json_free(guidance);
    json_free(uw_default);
    return out;
}
/* ── _LoopState dataclass construction. The instance projects as a PLAIN field
 *    dict (dataclasses.fields order), not an __attr__ wrapper. The _CTX_FIELDS
 *    slots are seeded from getattr(_ctx, name.lstrip("_")) in declaration order;
 *    system_message / moa_config / max_compression_attempts are explicit; the
 *    remaining slots take their dataclass defaults. ── */
/* PoP: cl2_loop_state @ agent/conversation_loop.py:_LoopState */
static json_t *cl2_loop_state(cl2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *ctx = cl2_get(in, "ctx");
    json_t *s = json_object();
#define CL2_CTX_FIELD(fname, ctxname) do { \
        json_t *v_ = cl2_req_attr(ctx, ctxname); \
        if (cl2_is_raise(v_)) { json_free(s); return v_; } \
        json_set(s, fname, json_copy(v_)); \
    } while (0)
#define CL2_EXPLICIT(fname) do { \
        json_t *v_ = cl2_get(in, fname); \
        json_set(s, fname, v_ ? json_copy(v_) : json_null()); \
    } while (0)
#define CL2_DEF_N(fname, num) json_set(s, fname, json_number(num))
#define CL2_DEF_B(fname, b)   json_set(s, fname, json_bool(b))
#define CL2_DEF_NULL(fname)   json_set(s, fname, json_null())
#define CL2_DEF_S(fname, str) json_set(s, fname, json_string(str))
    CL2_CTX_FIELD("user_message", "user_message");
    CL2_EXPLICIT("system_message");
    CL2_EXPLICIT("moa_config");
    CL2_CTX_FIELD("original_user_message", "original_user_message");
    CL2_CTX_FIELD("conversation_history", "conversation_history");
    CL2_CTX_FIELD("effective_task_id", "effective_task_id");
    CL2_CTX_FIELD("turn_id", "turn_id");
    CL2_CTX_FIELD("_should_review_memory", "should_review_memory");
    CL2_CTX_FIELD("_plugin_user_context", "plugin_user_context");
    CL2_CTX_FIELD("_ext_prefetch_cache", "ext_prefetch_cache");
    CL2_CTX_FIELD("messages", "messages");
    CL2_CTX_FIELD("active_system_prompt", "active_system_prompt");
    CL2_CTX_FIELD("current_turn_user_idx", "current_turn_user_idx");
    CL2_CTX_FIELD("_preflight_compression_blocked", "preflight_compression_blocked");
    CL2_EXPLICIT("max_compression_attempts");
    CL2_DEF_N("api_call_count", 0);
    CL2_DEF_NULL("final_response");
    CL2_DEF_B("interrupted", false);
    CL2_DEF_B("failed", false);
    CL2_DEF_N("codex_ack_continuations", 0);
    CL2_DEF_N("length_continue_retries", 0);
    CL2_DEF_N("restart_count", 0);
    CL2_DEF_N("_outer_error_count", 0);
    CL2_DEF_N("truncated_tool_call_retries", 0);
    json_set(s, "truncated_response_parts", json_array());
    CL2_DEF_N("compression_attempts", 0);
    CL2_DEF_NULL("_last_preflight_pressure");
    CL2_DEF_B("_provider_overflow_recovery_pending", false);
    CL2_DEF_B("_compression_timeout_exhausted", false);
    CL2_DEF_S("_turn_exit_reason", "unknown");
    CL2_DEF_NULL("_pending_verification_response");
    CL2_DEF_B("_pending_verification_response_previewed", false);
    CL2_DEF_NULL("pending_moa_prepared_request");
    CL2_DEF_NULL("request_logger");
    CL2_DEF_NULL("api_messages");
    CL2_DEF_NULL("tools_for_api");
    CL2_DEF_NULL("_moa_prepared_request");
    CL2_DEF_NULL("approx_tokens");
    CL2_DEF_NULL("request_pressure_tokens");
    CL2_DEF_NULL("total_chars");
    CL2_DEF_NULL("thinking_spinner");
    CL2_DEF_NULL("api_start_time");
    CL2_DEF_N("retry_count", 0);
    CL2_DEF_NULL("max_retries");
    CL2_DEF_NULL("_retry");
    CL2_DEF_S("finish_reason", "stop");
    CL2_DEF_NULL("response");
    CL2_DEF_NULL("api_kwargs");
    CL2_DEF_NULL("api_request_id");
    CL2_DEF_NULL("_original_api_kwargs");
    CL2_DEF_NULL("_llm_middleware_trace");
    CL2_DEF_NULL("api_duration");
    CL2_DEF_NULL("assistant_message");
#undef CL2_CTX_FIELD
#undef CL2_EXPLICIT
#undef CL2_DEF_N
#undef CL2_DEF_B
#undef CL2_DEF_NULL
#undef CL2_DEF_S
    json_set(st, "s", s);
    return json_copy(s);
}

/* ── seamed _run_phase contract: call the "run_phase" seam with (fn, agent,
 *    state, **extra), then copy the scripted verdict's fields back onto the
 *    loop-state plain dict (skip action/result; handle_api_error latch sets True
 *    only). Returns the verdict (caller-owned) or a raise spec. ── */
static json_t *cl2_run_phase(cl2_io_t *io, const json_t *fn, const json_t *agent,
                             json_t *sobj, const json_t *extra)
{
    json_t *a = cl2_args();
    cl2_arg_j(a, "fn", fn);
    cl2_arg_j(a, "agent", agent);
    cl2_arg_j(a, "state", sobj);
    if (cl2_is_obj(extra)) {
        for (size_t i = 0; i < extra->c.count; i++) {
            json_t *ev = extra->c.items[i];
            if (cl2_is_raise(ev)) json_set(a, extra->c.keys[i], json_string(cl2_raise_msg(ev)));
            else cl2_arg_j(a, extra->c.keys[i], ev);
        }
    }
    json_t *verdict = cl2_call(io, "run_phase", a);
    if (cl2_is_raise(verdict)) return verdict;
    /* copyback */
    const char *fname = cl2_str(cl2_get(fn, "__name__"));
    int latched = (fname && strcmp(fname, "handle_api_error") == 0);
    if (cl2_is_obj(verdict)) {
        for (size_t i = 0; i < verdict->c.count; i++) {
            const char *f = verdict->c.keys[i];
            if (strcmp(f, "action") == 0 || strcmp(f, "result") == 0) continue;
            json_t *value = verdict->c.items[i];
            int is_latched_field = latched && strcmp(f, "_provider_overflow_recovery_pending") == 0;
            if (!is_latched_field) json_set(sobj, f, json_copy(value));
            else if (cl2_truth(value)) json_set(sobj, f, json_bool(true));
        }
    }
    return verdict;
}

/* phase fn double builder: {"__callable__": name, "__name__": name} */
static json_t *cl2_phase_fn(const char *name)
{
    json_t *o = json_object();
    json_set(o, "__callable__", json_string(name));
    json_set(o, "__name__", json_string(name));
    return o;
}

/* action string helper (borrowed). */
static const char *cl2_action(const json_t *verdict)
{
    return cl2_str(cl2_get(verdict, "action"));
}
static int cl2_action_is(const json_t *verdict, const char *want)
{
    const char *a = cl2_action(verdict);
    return a && strcmp(a, want) == 0;
}
/* loop-condition helper: 1=enter, 0=exit, -1=raise (*raiser owned). Transcribes
 * `_prelude_action != "break" and ((s.api_call_count < agent.max_iterations and
 *   agent.iteration_budget.remaining > 0) or agent._budget_grace_call)`. */
static int cl2_loop_cond(json_t *sobj, const json_t *agentw, const char *pa, json_t **raiser)
{
    *raiser = NULL;
    if (pa && strcmp(pa, "break") == 0) return 0;
    double api_count = cl2_num(json_obj_get(sobj, "api_call_count"));
    json_t *max_iter = cl2_req_attr(agentw, "max_iterations");
    if (cl2_is_raise(max_iter)) { *raiser = max_iter; return -1; }
    int and_chain = 0;
    if (api_count < cl2_num(max_iter)) {
        json_t *budget = cl2_req_attr(agentw, "iteration_budget");
        if (cl2_is_raise(budget)) { *raiser = budget; return -1; }
        json_t *remaining = cl2_req_attr(budget, "remaining");
        if (cl2_is_raise(remaining)) { *raiser = remaining; return -1; }
        and_chain = cl2_num(remaining) > 0.0;
    }
    if (and_chain) return 1;
    json_t *grace = cl2_req_attr(agentw, "_budget_grace_call");
    if (cl2_is_raise(grace)) { *raiser = grace; return -1; }
    return cl2_truth(grace) ? 1 : 0;
}

/* PoP: cl2_run_conversation_turn @ agent/conversation_loop.py:_run_conversation_turn */
static json_t *cl2_run_conversation_turn(cl2_io_t *io, const json_t *in, json_t *st)
{
    json_t *ret = NULL;
    cl2_self(st, "agent"); /* vivify wrapper */
    json_t *agentw = cl2_get(st, "agent");

    json_t *um = cl2_copy_or_null(cl2_get(in, "user_message"));
    json_t *mc = cl2_copy_or_null(cl2_get(in, "moa_config"));
    json_t *pum = cl2_copy_or_null(cl2_get(in, "persist_user_message"));
    json_t *ctx = NULL;
    json_t *pa = NULL, *pr = NULL;

    /* if moa_config is None: user_message, moa_config, persist_user_message =
     *     _decode_inline_moa_turn(user_message, persist_user_message) */
    if (cl2_is_null(mc)) {
        json_t *a = cl2_args();
        cl2_arg_j(a, "user_message", um);
        cl2_arg_j(a, "persist_user_message", pum);
        json_t *dec = cl2_call(io, "decode_inline_moa_turn", a);
        if (cl2_is_raise(dec)) { ret = dec; goto cleanup; }
        if (!cl2_is_arr(dec) || json_len(dec) != 3) {
            size_t got = cl2_is_arr(dec) ? json_len(dec) : 0;
            char *m;
            if (cl2_is_arr(dec) && got > 3) m = cl2_xfmt("too many values to unpack (expected 3)");
            else m = cl2_xfmt("not enough values to unpack (expected 3, got %zu)", got);
            ret = cl2_mkraise("ValueError", m ? m : "");
            free(m); json_free(dec); goto cleanup;
        }
        json_free(um); json_free(mc); json_free(pum);
        um = json_copy(json_get(dec, 0));
        mc = json_copy(json_get(dec, 1));
        pum = json_copy(json_get(dec, 2));
        json_free(dec);
    }

    /* agent._last_compaction_in_place = agent._last_compression_attempt_recorded = False */
    cl2_dset(agentw, "_last_compaction_in_place", json_bool(false));
    cl2_dset(agentw, "_last_compression_attempt_recorded", json_bool(false));
    cl2_dset(agentw, "_last_compression_attempt_in_place", json_null());

    /* begin_fast_mode_turn(agent, conversation_history) */
    {
        json_t *a = cl2_args();
        cl2_arg_j(a, "agent", agentw);
        cl2_arg_j(a, "conversation_history", cl2_get(in, "conversation_history"));
        json_t *r = cl2_call(io, "begin_fast_mode_turn", a);
        if (cl2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }

    /* try: agent._try_refresh_env_client_credentials()
     * except Exception: logger.debug("per-turn env credential refresh failed", exc_info=True) */
    {
        json_t *r = cl2_call(io, "try_refresh_env_client_credentials", cl2_args());
        if (cl2_is_raise(r)) {
            if (!cl2_exc_is_exception(cl2_kind(r))) { ret = r; goto cleanup; }
            json_free(r);
            json_t *lr = cl2_log(io, "debug", "per-turn env credential refresh failed", NULL, 1);
            if (cl2_is_raise(lr)) { ret = lr; goto cleanup; }
            json_free(lr);
        } else {
            json_free(r);
        }
    }

    /* agent._held_transforms = held_stream_transforms(); agent._hold_stream = bool(...) */
    {
        json_t *ht = cl2_call(io, "held_stream_transforms", cl2_args());
        if (cl2_is_raise(ht)) { ret = ht; goto cleanup; }
        int hold = cl2_truth(ht);
        cl2_dset(agentw, "_held_transforms", ht);
        cl2_dset(agentw, "_hold_stream", json_bool(hold ? true : false));
    }

    /* try: _ctx = build_turn_context(...) except PreflightCompressionTimedOut: ... */
    {
        json_t *a = cl2_args();
        cl2_arg_j(a, "agent", agentw);
        cl2_arg_j(a, "user_message", um);
        cl2_arg_j(a, "system_message", cl2_get(in, "system_message"));
        cl2_arg_j(a, "conversation_history", cl2_get(in, "conversation_history"));
        cl2_arg_j(a, "task_id", cl2_get(in, "task_id"));
        cl2_arg_j(a, "stream_callback", cl2_get(in, "stream_callback"));
        cl2_arg_j(a, "persist_user_message", pum);
        cl2_arg_j(a, "persist_user_timestamp", cl2_get(in, "persist_user_timestamp"));
        cl2_arg_j(a, "persist_user_display_kind", cl2_get(in, "persist_user_display_kind"));
        cl2_arg_j(a, "persist_user_display_metadata", cl2_get(in, "persist_user_display_metadata"));
        cl2_arg_j(a, "persist_user_platform_id", cl2_get(in, "persist_user_platform_id"));
        cl2_arg_j(a, "turn_author", cl2_get(in, "turn_author"));
        json_set(a, "restore_or_build_system_prompt", cl2_phase_fn("restore_or_build_system_prompt"));
        json_set(a, "install_safe_stdio", cl2_phase_fn("install_safe_stdio"));
        json_set(a, "sanitize_surrogates", cl2_phase_fn("sanitize_surrogates"));
        json_set(a, "summarize_user_message_for_log", cl2_phase_fn("summarize_user_message_for_log"));
        json_set(a, "set_session_context", cl2_phase_fn("set_session_context"));
        json_set(a, "set_current_write_origin", cl2_phase_fn("set_current_write_origin"));
        json_set(a, "ra", cl2_phase_fn("ra"));
        cl2_arg_b(a, "moa_active", cl2_truth(mc));
        cl2_arg_j(a, "title_user_message", cl2_get(in, "title_user_message"));
        json_t *r = cl2_call(io, "build_turn_context", a);
        if (cl2_is_raise(r)) {
            if (cl2_exc_is_instance(cl2_kind(r), "PreflightCompressionTimedOut")) {
                /* return _preflight_timeout_result(agent, exc, conversation_history) */
                json_t *fa = cl2_args();
                cl2_arg_j(fa, "agent", agentw);
                cl2_arg_s(fa, "exc", cl2_raise_msg(r));
                cl2_arg_j(fa, "conversation_history", cl2_get(in, "conversation_history"));
                json_free(r);
                json_t *fr = cl2_call(io, "preflight_timeout_result", fa);
                ret = fr; /* raise or value, returned as-is */
                goto cleanup;
            }
            ret = r; goto cleanup;
        }
        ctx = r;
    }

    /* _ctx.active_system_prompt = begin_voice_turn_route(agent, _ctx.messages, _ctx.active_system_prompt) */
    {
        json_t *msgs = cl2_req_attr(ctx, "messages");
        if (cl2_is_raise(msgs)) { ret = msgs; goto cleanup; }
        json_t *asp = cl2_req_attr(ctx, "active_system_prompt");
        if (cl2_is_raise(asp)) { ret = asp; goto cleanup; }
        json_t *a = cl2_args();
        cl2_arg_j(a, "agent", agentw);
        cl2_arg_j(a, "messages", msgs);
        cl2_arg_j(a, "active_system_prompt", asp);
        json_t *r = cl2_call(io, "begin_voice_turn_route", a);
        if (cl2_is_raise(r)) { ret = r; goto cleanup; }
        cl2_dset(ctx, "active_system_prompt", r);
    }

    /* per-turn agent state resets */
    cl2_dset(agentw, "_delivered_interim_texts", json_array());
    cl2_dset(agentw, "_incremental_persistence_failed", json_bool(false));
    cl2_dset(agentw, "_last_persistence_error_cause", json_null());
    cl2_dset(agentw, "_compression_adoption_failed", json_bool(false));
    cl2_dset(agentw, "_ephemeral_reasoning_off", json_bool(false));
    cl2_dset(agentw, "_auth_pool_refresh_counts", json_object());
    cl2_dset(agentw, "_last_turn_usage", json_null());

    /* s = _LoopState(system_message=..., moa_config=..., max_compression_attempts=getattr(agent,...,3), **ctx) */
    {
        json_t *def3 = json_number(3);
        json_t *mca = cl2_getattr_def(agentw, "max_compression_attempts", def3);
        json_free(def3);
        json_t *pin = json_object();
        cl2_arg_j(pin, "system_message", cl2_get(in, "system_message"));
        cl2_arg_j(pin, "moa_config", mc);
        json_set(pin, "max_compression_attempts", mca);
        json_set(pin, "ctx", json_copy(ctx));
        json_t *sr = cl2_loop_state(io, pin, st);
        json_free(pin);
        if (cl2_is_raise(sr)) { ret = sr; goto cleanup; }
        json_free(sr); /* st.s now holds the constructed state */
    }
    json_t *sobj = cl2_get(st, "s");

    /* if (early_result := _turn_without_loop(agent, s, _ctx.plugin_block_message)) is not None: return */
    {
        json_t *pbm = cl2_req_attr(ctx, "plugin_block_message");
        if (cl2_is_raise(pbm)) { ret = pbm; goto cleanup; }
        json_t *a = cl2_args();
        cl2_arg_j(a, "agent", agentw);
        cl2_arg_j(a, "s", sobj);
        cl2_arg_j(a, "block_message", pbm);
        json_t *er = cl2_call(io, "turn_without_loop", a);
        if (cl2_is_raise(er)) { ret = er; goto cleanup; }
        if (!cl2_is_null(er)) { ret = er; goto cleanup; }
        json_free(er);
    }

    /* _prelude_action, _prelude_result = play_prelude(agent, s, prelude) */
    {
        json_t *a = cl2_args();
        cl2_arg_j(a, "agent", agentw);
        cl2_arg_j(a, "s", sobj);
        cl2_arg_j(a, "prelude", cl2_get(in, "prelude"));
        json_t *pl = cl2_call(io, "play_prelude", a);
        if (cl2_is_raise(pl)) { ret = pl; goto cleanup; }
        if (!cl2_is_arr(pl) || json_len(pl) != 2) {
            size_t got = cl2_is_arr(pl) ? json_len(pl) : 0;
            char *m;
            if (cl2_is_arr(pl) && got > 2) m = cl2_xfmt("too many values to unpack (expected 2)");
            else m = cl2_xfmt("not enough values to unpack (expected 2, got %zu)", got);
            ret = cl2_mkraise("ValueError", m ? m : "");
            free(m); json_free(pl); goto cleanup;
        }
        pa = json_copy(json_get(pl, 0));
        pr = json_copy(json_get(pl, 1));
        json_free(pl);
    }
    /* if _prelude_action == "return": return _prelude_result */
    if (cl2_is_str(pa) && strcmp(cl2_str(pa), "return") == 0) {
        ret = pr; pr = NULL; goto cleanup;
    }

    /* while ... */
    for (;;) {
        json_t *raiser = NULL;
        const char *pa_s = cl2_str(pa);
        int cond = cl2_loop_cond(sobj, agentw, pa_s, &raiser);
        if (cond == -1) { ret = raiser; goto cleanup; }
        if (cond == 0) break;

        json_t *v;
        /* begin_iteration */
        v = cl2_run_phase(io, cl2_phase_fn("begin_iteration"), agentw, sobj, NULL);
        if (cl2_is_raise(v)) { ret = v; goto cleanup; }
        if (cl2_action_is(v, "break")) { json_free(v); break; }
        json_free(v);
        /* prepare_iteration / route_change_offload / assemble_api_request */
        v = cl2_run_phase(io, cl2_phase_fn("prepare_iteration"), agentw, sobj, NULL);
        if (cl2_is_raise(v)) { ret = v; goto cleanup; }
        json_free(v);
        v = cl2_run_phase(io, cl2_phase_fn("route_change_offload"), agentw, sobj, NULL);
        if (cl2_is_raise(v)) { ret = v; goto cleanup; }
        json_free(v);
        v = cl2_run_phase(io, cl2_phase_fn("assemble_api_request"), agentw, sobj, NULL);
        if (cl2_is_raise(v)) { ret = v; goto cleanup; }
        json_free(v);
        /* run_preflight_gate */
        v = cl2_run_phase(io, cl2_phase_fn("run_preflight_gate"), agentw, sobj, NULL);
        if (cl2_is_raise(v)) { ret = v; goto cleanup; }
        if (cl2_action_is(v, "return")) { ret = cl2_copy_or_null(cl2_get(v, "result")); json_free(v); goto cleanup; }
        if (cl2_action_is(v, "break")) { json_free(v); break; }
        if (cl2_action_is(v, "continue")) { json_free(v); continue; }
        json_free(v);
        /* announce_api_call */
        v = cl2_run_phase(io, cl2_phase_fn("announce_api_call"), agentw, sobj, NULL);
        if (cl2_is_raise(v)) { ret = v; goto cleanup; }
        json_free(v);

        /* s.api_start_time, s.retry_count, s.max_retries = time.time(), 0, agent._api_max_retries */
        {
            json_t *t = cl2_call(io, "time", cl2_args());
            if (cl2_is_raise(t)) { ret = t; goto cleanup; }
            json_t *mr = cl2_req_attr(agentw, "_api_max_retries");
            if (cl2_is_raise(mr)) { json_free(t); ret = mr; goto cleanup; }
            json_set(sobj, "api_start_time", t);
            json_set(sobj, "retry_count", json_number(0));
            json_set(sobj, "max_retries", json_copy(mr));
        }
        /* s._retry, s.finish_reason, s.response, s.api_kwargs = TurnRetryState(), "stop", None, None */
        {
            json_t *tr = cl2_call(io, "turn_retry_state", cl2_args());
            if (cl2_is_raise(tr)) { ret = tr; goto cleanup; }
            json_set(sobj, "_retry", tr);
            json_set(sobj, "finish_reason", json_string("stop"));
            json_set(sobj, "response", json_null());
            json_set(sobj, "api_kwargs", json_null());
        }
        /* s.api_request_id = agent._current_api_request_id = f"{s.turn_id}:api:{s.api_call_count}" */
        {
            char *tids = cl2_pystr(json_obj_get(sobj, "turn_id"));
            char *acs = cl2_pystr(json_obj_get(sobj, "api_call_count"));
            char *rid = cl2_xfmt("%s:api:%s", tids ? tids : "", acs ? acs : "");
            free(tids); free(acs);
            json_t *node = json_string(rid ? rid : "");
            free(rid);
            json_set(sobj, "api_request_id", node);
            cl2_dset(agentw, "_current_api_request_id", json_copy(node));
        }

        /* early_result = _run_api_retry_loop(agent, s); if not None: return */
        {
            json_t *a = cl2_args();
            cl2_arg_j(a, "agent", agentw);
            cl2_arg_j(a, "s", sobj);
            json_t *er = cl2_call(io, "run_api_retry_loop", a);
            if (cl2_is_raise(er)) { ret = er; goto cleanup; }
            if (!cl2_is_null(er)) { ret = er; goto cleanup; }
            json_free(er);
        }

        /* apply_retry_restarts */
        v = cl2_run_phase(io, cl2_phase_fn("apply_retry_restarts"), agentw, sobj, NULL);
        if (cl2_is_raise(v)) { ret = v; goto cleanup; }
        if (cl2_action_is(v, "break")) { json_free(v); break; }
        if (cl2_action_is(v, "continue")) { json_free(v); continue; }
        json_free(v);

        /* try: normalize_model_response -> run_tool_round / finish_text_response
         * except Exception as e: handle_outer_loop_error(e=e) */
        {
            json_t *outer_exc = NULL;
            json_t *ri = cl2_run_phase(io, cl2_phase_fn("normalize_model_response"), agentw, sobj, NULL);
            if (cl2_is_raise(ri)) { outer_exc = ri; goto outer_handle; }
            if (cl2_action_is(ri, "return")) { ret = cl2_copy_or_null(cl2_get(ri, "result")); json_free(ri); goto cleanup; }
            if (cl2_action_is(ri, "continue")) { json_free(ri); continue; }
            json_free(ri);
            json_t *am = json_obj_get(sobj, "assistant_message");
            json_t *tc = cl2_req_attr(am, "tool_calls");
            if (cl2_is_raise(tc)) { outer_exc = tc; goto outer_handle; }
            const char *phase = cl2_truth(tc) ? "run_tool_round" : "finish_text_response";
            json_t *fv = cl2_run_phase(io, cl2_phase_fn(phase), agentw, sobj, NULL);
            if (cl2_is_raise(fv)) { outer_exc = fv; goto outer_handle; }
            if (cl2_action_is(fv, "return")) { ret = cl2_copy_or_null(cl2_get(fv, "result")); json_free(fv); goto cleanup; }
            if (cl2_action_is(fv, "break")) { json_free(fv); break; }
            if (cl2_action_is(fv, "continue")) { json_free(fv); continue; }
            json_free(fv);
            goto after_outer;
        outer_handle:
            if (!cl2_exc_is_exception(cl2_kind(outer_exc))) { ret = outer_exc; goto cleanup; }
            {
                json_t *extra = json_object();
                json_set(extra, "e", outer_exc); /* ownership transferred */
                json_t *he = cl2_run_phase(io, cl2_phase_fn("handle_outer_loop_error"), agentw, sobj, extra);
                json_free(extra);
                if (cl2_is_raise(he)) { ret = he; goto cleanup; }
                if (cl2_action_is(he, "break")) { json_free(he); break; }
                json_free(he);
            }
        after_outer:
            ;
        }
    }

    /* result = _finalize_from_state(agent, s) */
    {
        json_t *a = cl2_args();
        cl2_arg_j(a, "agent", agentw);
        cl2_arg_j(a, "s", sobj);
        json_t *result = cl2_call(io, "finalize_from_state", a);
        if (cl2_is_raise(result)) { ret = result; goto cleanup; }
        /* if s._compression_timeout_exhausted: result.update(error=..., partial=True, compression_exhausted=True) */
        if (cl2_truth(json_obj_get(sobj, "_compression_timeout_exhausted"))) {
            if (cl2_is_obj(result)) {
                json_set(result, "error", json_string(CL2_COMPRESSION_TIMEOUT_FINAL_RESPONSE));
                json_set(result, "partial", json_bool(true));
                json_set(result, "compression_exhausted", json_bool(true));
            }
        }
        ret = result;
    }

cleanup:
    json_free(um);
    json_free(mc);
    json_free(pum);
    json_free(ctx);
    json_free(pa);
    json_free(pr);
    return ret;
}