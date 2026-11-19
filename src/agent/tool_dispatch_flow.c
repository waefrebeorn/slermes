/* tool_dispatch_flow.c — FLOW port of tools/delegate_tool_dispatch.py (LIVE
 * authority). The task label "agent/tool_dispatch.py" does not exist in the
 * quarry; the live tool-dispatch surface (real-gap-list "18 gaps" module) is
 * tools/delegate_tool_dispatch.py. Ports 21 UNCLAIMED functions: the _Batch
 * owner_kwargs / run_child methods, the batch announce (_announce_batch) and
 * origin capture (_capture_origin), the child-done reporter (_report_child_done)
 * and finished-child recorder (_record_finished_child), the resume probe
 * (_waiting_on_resume), the unstarted release (_release_unstarted_child) and
 * gated run (_run_gated), the parallel children runner (_run_children_parallel),
 * the execute-and-aggregate join (_execute_and_aggregate), the sync fallback
 * note (_run_sync_with_note), the async wake-sid / session-key resolvers
 * (_resolve_async_wake_sid / _resolve_async_session_key), the progress token
 * (_batch_progress_token), the dispatched payload (_dispatched_payload), the
 * unit partition (_units_of), the unit dispatch (_dispatch_unit), the
 * parent-cancellation restore (_restore_parent_cancellation), the background
 * dispatcher (_dispatch_background) and the detached-only refusal
 * (_refuse_detached_only).
 *
 * Claimed sibling _run_batch (src/tools/clarify_tool.c PoP) is NOT ported; it
 * and all foreign I/O enter through the td_io_t universal seam. Ported siblings
 * compose for REAL (_release_unstarted_child inside _run_gated /
 * _refuse_detached_only, _record_finished_child / _report_child_done /
 * _waiting_on_resume inside _run_children_parallel, _run_children_parallel /
 * _Batch.run_child inside _execute_and_aggregate, _execute_and_aggregate inside
 * _run_sync_with_note / _dispatch_background, _resolve_async_wake_sid /
 * _resolve_async_session_key / _units_of / _dispatch_unit /
 * _restore_parent_cancellation / _refuse_detached_only / _dispatched_payload
 * inside _dispatch_background).
 *
 * Decision sequences only: Python truthiness, `or`/`and` short-circuits, `is
 * None` / `is True` / `is False` identity vs truthy tests, kwarg/operand
 * evaluation order, dict insertion order, and except-Exception catch scopes are
 * transcribed per call site. A scripted BaseException (SystemExit /
 * KeyboardInterrupt / GeneratorExit) propagates past every except Exception
 * guard exactly as live (full builtin exception-parent table below).
 *
 * Value model (shared with tests/oracle_tool_dispatch_flow.py):
 *   - attribute doubles {"__attr__": {...}} <-> SimpleNamespace; the state
 *     window keeps the wrapper form and is mutated in place. _Batch is a PLAIN
 *     object (dataclass norm -> field dict); the harness fills the dataclass
 *     defaults (group/unit_id/live_home=null, refused=[], detached_only=false).
 *   - futures are per-item scripted doubles: the executor.submit seam result is
 *     a spec {"id","done","result","cancelled"}; done/result/cancelled scripts
 *     live ON the spec (single value reused, bare array = per-call queue) and
 *     record future.done / future.result / future.cancelled with args {"id"}.
 *   - callables project to {"__callable__": true}.
 *   - raise specs {"__raise__": "<ClassName>", "message": str(e)}.
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - json.dumps returns (_run_sync_with_note / _dispatch_background) carry NO
 *     integral floats (libjson has no int/float identity); total_duration is
 *     scripted non-integral and small. Entry duration_seconds is int /
 *     non-integral float. round(x,2) fixtures avoid exact half-way decimals.
 *   - _run_children_parallel pending sets iterate in ascending future-id order
 *     (oracle FutureDouble.__hash__ = id suffix; ids "fN", N = child position,
 *     <8); the initial pending is modeled as children order; cf_wait returns
 *     scripted ordered lists.
 *   - entries are plain dicts; task_index / goal / model keys present where the
 *     live code subscripts them (KeyError cases script the absence).
 *   - the live emoji literals (U+1F500 U+26A0 U+2713 U+2717 U+00B7 U+2014) are
 *     hex-escaped; fixture text is ASCII.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified "agent/tool_dispatch_flow.c". Every
 * symbol is static td_*.
 *
 * Gate: bash tests/run_one_oracle.sh tool_dispatch_flow \
 *           oracle_tool_dispatch_flow.py "" "lib/libjson/json.o"
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

/* ── live module constants (byte-pinned to tools/delegate_tool_dispatch.py and
 *    tools/delegate_tool_progress.py) ── */
/* SUBAGENT_FAILURE_STATUSES = frozenset({"failed","error","timeout"}); membership only. */
/* U+1F500 🔀, U+26A0 ⚠, U+2713 ✓, U+2717 ✗, U+00B7 ·, U+2014 em dash. */
#define TD_SHUFFLE "\xf0\x9f\x94\x80"
#define TD_WARN    "\xe2\x9a\xa0"
#define TD_CHECK   "\xe2\x9c\x93"
#define TD_XMARK   "\xe2\x9c\x97"
#define TD_MIDDOT  "\xc2\xb7"
#define TD_EMDASH  "\xe2\x80\x94"

/* _SYNC_FALLBACK_NOTES / _BACKGROUND_NOTES (byte-pinned). */
#define TD_NOTE_NO_ASYNC \
    "background=true is not available in this session " TD_EMDASH " it cannot " \
    "receive a detached subagent result after the turn ends (a " \
    "finite chat using -Q, --oneshot, or non-TTY stdio, `hermes -z`, a /bg background task, a cron " \
    "job, a Kanban worker, or a stateless HTTP endpoint). The subagent(s) ran SYNCHRONOUSLY and the result is included above."
#define TD_NOTE_AT_CAPACITY \
    "The background delegation pool was at capacity (delegation.max_concurrent_children), so the subagent(s) ran " \
    "SYNCHRONOUSLY and the result is included above. Raise " \
    "delegation.max_concurrent_children in config.yaml to allow more concurrent background delegations."
#define TD_NOTE_ONE \
    "Subagent is running in the background; its full result re-enters the conversation as a new message when it " \
    "finishes. Results are delivered only after you END YOUR TURN: do anything that does not depend on it, then " \
    "stop with a one-line status. Do not poll its transcript or artifacts to wait for it."
#define TD_NOTE_MANY \
    "%s subagents are running in parallel in the background as %s completion unit(s); each unit's results " \
    "re-enter the conversation as their own new message when THAT unit finishes. Results are delivered only " \
    "after you END YOUR TURN: do anything that does not depend on them, then stop with a one-line status. Do not " \
    "poll transcripts or artifacts to wait for them."
#define TD_NOTE_CONTROL_HINT \
    "While a child runs you can orchestrate it live with this same tool: delegate_task(action='list') to see live " \
    "children, action='steer' with subagent_id + message to redirect one, action='stop' with subagent_id to end " \
    "one early."
#define TD_NOTE_LIVE_TRANSCRIPTS_HINT \
    "Each subagent streams a human-readable transcript of its operations to the file listed above (append-only, " \
    "one per task). Read or `tail -f` these paths at any time to watch a child work while it runs."

/* ── universal seam ── */
typedef struct td_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} td_io_t;

/* ── truthiness / accessors ── */
static int td_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int td_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int td_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int td_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int td_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int td_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int td_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *td_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *td_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double td_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* isinstance(x, dict): a PLAIN object, never an __attr__ double. */
static int td_is_plain_dict(const json_t *v)
{
    return td_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

/* isinstance(x, int): a number OR a bool (bool is an int subclass). Integral
 * floats read as int in libjson; the fixture domain keeps them integral-int. */
static int td_is_int(const json_t *v)
{
    if (td_is_bool(v)) return 1;
    if (!td_is_num(v)) return 0;
    double d = v->num_val;
    return isfinite(d) && d == (double)(long long)d;
}

static const char *td_typename(const json_t *v)
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
            const char *tag = td_str(td_get(v, "__class__"));
            if (tag) return tag;
            return json_obj_get(v, "__attr__") ? "types.SimpleNamespace" : "dict";
        }
        default: return "object";
    }
}

/* ── attribute doubles ── */
static json_t *td_dattr(const json_t *d, const char *name)
{
    return td_get(td_get(d, "__attr__"), name);
}

/* getattr(d, name, default): a wrapper reads __attr__.name; None / non-wrapper
 * yields the default (getattr never raises when a default is given). */
static json_t *td_getattr(const json_t *d, const char *name, json_t *defval)
{
    json_t *inner = td_get(d, "__attr__");
    if (inner) {
        json_t *v = json_obj_get(inner, name);
        return v ? v : defval;
    }
    return defval;
}

static void td_dset(const json_t *d, const char *name, json_t *val)
{
    json_t *inner = td_get(d, "__attr__");
    if (inner && inner->type == JSON_OBJECT) json_set(inner, name, val);
    else json_free(val);
}

/* ── raise specs ── */
static int td_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *td_kind(const json_t *r)
{
    const char *k = td_str(td_get(r, "__raise__"));
    return k ? k : "";
}

static const char *td_raise_msg(const json_t *r)
{
    const char *m = td_str(td_get(r, "message"));
    return m ? m : "";
}

static json_t *td_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *td_xfmt(const char *fmt, ...)
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

static json_t *td_raise_keyerr(const char *key)
{
    char *m = td_xfmt("'%s'", key);
    json_t *r = td_mkraise("KeyError", m ? m : key);
    free(m);
    return r;
}

static json_t *td_raise_attr(const json_t *obj, const char *name)
{
    char *m = td_xfmt("'%s' object has no attribute '%s'", td_typename(obj), name);
    json_t *r = td_mkraise("AttributeError", m ? m : "");
    free(m);
    return r;
}

/* ── FULL builtin exception-parent hierarchy (py3.14). Walk to test
 *    except Exception / except <Class> ancestry. The BaseException trio (and
 *    asyncio.CancelledError) parent to BaseException ONLY, so they pass through
 *    every except Exception. ── */
typedef struct { const char *name; const char *parent; } td_exc_row;
static const td_exc_row TD_EXC_PARENTS[] = {
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

static const char *td_exc_parent(const char *name)
{
    for (size_t i = 0; TD_EXC_PARENTS[i].name; i++)
        if (strcmp(TD_EXC_PARENTS[i].name, name) == 0) return TD_EXC_PARENTS[i].parent;
    return "Exception";
}

static int td_exc_is_instance(const char *kind, const char *base)
{
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, base) == 0) return 1;
        const char *p = td_exc_parent(cur);
        if (!p || strcmp(cur, p) == 0) break;
        snprintf(cur, sizeof(cur), "%s", p);
        if (strcmp(cur, "BaseException") == 0) return strcmp(base, "BaseException") == 0;
    }
    return 0;
}

static int td_exc_is_exception(const char *kind)
{
    return td_exc_is_instance(kind, "Exception");
}

/* ── universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. ── */
static json_t *td_call(td_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = td_xfmt("no seam installed for %s", name);
        r = td_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *td_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides. */
static void td_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void td_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void td_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void td_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* double projection for a seam arg: a wrapper copies as-is (C wrappers never
 * carry injected method keys); anything else copies verbatim. */
static json_t *td_double_proj(const json_t *v)
{
    return v ? json_copy(v) : json_null();
}

/* children triples -> [[i, task, child], ...] projection. */
static json_t *td_triple_proj(const json_t *children)
{
    json_t *out = json_array();
    if (!td_is_arr(children)) return out;
    for (size_t i = 0; i < json_len(children); i++) {
        json_t *tr = json_get(children, i);
        json_t *row = json_array();
        json_append(row, json_copy(json_get(tr, 0)));
        json_append(row, json_copy(json_get(tr, 1)));
        json_append(row, td_double_proj(json_get(tr, 2)));
        json_append(out, row);
    }
    return out;
}

/* ── string helpers (ASCII domain) ── */
static json_t *td_strn(const char *s, size_t n)
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
static char *td_float_repr(double d)
{
    char buf[64];
    for (int p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*g", p, d);
        char *end = NULL;
        double back = strtod(buf, &end);
        if (end && *end == '\0' && back == d) return td_xfmt("%s", buf);
    }
    snprintf(buf, sizeof(buf), "%.17g", d);
    return td_xfmt("%s", buf);
}

/* Python str() projection (domain: str / number / bool / None). */
static char *td_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return td_xfmt("None");
    if (v->type == JSON_BOOL) return td_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18) return td_xfmt("%.0f", d);
        return td_float_repr(d);
    }
    if (v->type == JSON_STRING) return td_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : td_xfmt("");
}

/* round(x, 2) — correctly-rounded 2-decimal double (default FE_TONEAREST). */
static double td_round2(double x)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", x);
    return strtod(buf, NULL);
}

/* Python list indexing with negative wrap; NULL when out of range. */
static json_t *td_list_get(const json_t *arr, double idx)
{
    if (!td_is_arr(arr)) return NULL;
    long long n = (long long)json_len(arr);
    long long i = (long long)idx;
    if (i < 0) i += n;
    if (i < 0 || i >= n) return NULL;
    return json_get(arr, (size_t)i);
}

/* ── json.dumps(result, ensure_ascii=False) — Python separators (", ", ": "),
 *    insertion order, shortest float repr, JSON string escaping. Domain: no
 *    integral floats / non-finite numbers in dumps positions. ── */
typedef struct { char *b; size_t n, cap; } td_ser;
static void td_ser_init(td_ser *s) { s->cap = 128; s->n = 0; s->b = (char *)malloc(s->cap); if (s->b) s->b[0] = '\0'; }
static void td_ser_put(td_ser *s, const char *t)
{
    if (!s->b) return;
    size_t l = strlen(t);
    if (s->n + l + 1 > s->cap) {
        while (s->n + l + 1 > s->cap) s->cap *= 2;
        char *nb = (char *)realloc(s->b, s->cap);
        if (!nb) { free(s->b); s->b = NULL; return; }
        s->b = nb;
    }
    memcpy(s->b + s->n, t, l + 1);
    s->n += l;
}
static void td_ser_putc(td_ser *s, char c) { char t[2] = {c, '\0'}; td_ser_put(s, t); }
static void td_ser_str(td_ser *s, const char *str)
{
    td_ser_putc(s, '"');
    if (!str) { td_ser_putc(s, '"'); return; }
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  td_ser_put(s, "\\\""); break;
            case '\\': td_ser_put(s, "\\\\"); break;
            case '\n': td_ser_put(s, "\\n"); break;
            case '\r': td_ser_put(s, "\\r"); break;
            case '\t': td_ser_put(s, "\\t"); break;
            case '\b': td_ser_put(s, "\\b"); break;
            case '\f': td_ser_put(s, "\\f"); break;
            default:
                if (c < 0x20) { char u[8]; snprintf(u, sizeof(u), "\\u%04x", c); td_ser_put(s, u); }
                else td_ser_putc(s, (char)c);
        }
    }
    td_ser_putc(s, '"');
}
static void td_ser_node(td_ser *s, const json_t *v)
{
    if (!v || v->type == JSON_NULL) { td_ser_put(s, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: td_ser_put(s, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18) {
                char t[64]; snprintf(t, sizeof(t), "%.0f", d); td_ser_put(s, t);
            } else { char *t = td_float_repr(d); td_ser_put(s, t ? t : "0"); free(t); }
            break;
        }
        case JSON_STRING: td_ser_str(s, v->str_val); break;
        case JSON_ARRAY:
            td_ser_putc(s, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) td_ser_put(s, ", ");
                td_ser_node(s, v->c.items[i]);
            }
            td_ser_putc(s, ']');
            break;
        case JSON_OBJECT:
            td_ser_putc(s, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) td_ser_put(s, ", ");
                td_ser_str(s, v->c.keys[i]);
                td_ser_put(s, ": ");
                td_ser_node(s, v->c.items[i]);
            }
            td_ser_putc(s, '}');
            break;
        default: td_ser_put(s, "null");
    }
}
static char *td_json_dumps(const json_t *v)
{
    td_ser s; td_ser_init(&s);
    td_ser_node(&s, v);
    return s.b;
}

/* ── logger seam (args recorded VERBATIM; %s/%r apply only at emission) ── */
static json_t *td_logger(td_io_t *io, const char *level, const char *msg, json_t *args_arr, int exc_info)
{
    json_t *a = td_args();
    td_arg_s(a, "msg", msg);
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) td_arg_b(a, "exc_info", 1);
    char *nm = td_xfmt("logger.%s", level);
    json_t *r = td_call(io, nm ? nm : "logger.info", a);
    free(nm);
    return r;
}

/* _quiet exit: swallow an Exception raise; a non-None log_message is logged at
 * debug with the exception filling a trailing unsatisfied %s. Returns NULL when
 * swallowed, or the logger.debug raise spec if that call itself raises. A
 * BaseException is NOT swallowed (caller propagates). */
static json_t *td_quiet_swallow(td_io_t *io, const char *msg, json_t *r, int exc_info)
{
    if (!td_exc_is_exception(td_kind(r))) return r;  /* BaseException passes */
    if (!msg) { json_free(r); return NULL; }
    int pct = 0;
    for (const char *p = msg; *p; p++) if (*p == '%') pct++;
    json_t *la = json_array();
    if (pct > 0) json_append(la, json_string(td_raise_msg(r)));
    json_free(r);
    return td_logger(io, "debug", msg, la, exc_info);
}

/* ── _Batch field defaults (dataclass norm fills these) ── */
static void td_batch_defaults(json_t *b)
{
    if (!td_is_obj(b)) return;
    if (!json_obj_get(b, "group")) json_set(b, "group", json_null());
    if (!json_obj_get(b, "unit_id")) json_set(b, "unit_id", json_null());
    if (!json_obj_get(b, "live_home")) json_set(b, "live_home", json_null());
    if (!json_obj_get(b, "refused")) json_set(b, "refused", json_array());
    if (!json_obj_get(b, "detached_only")) json_set(b, "detached_only", json_bool(false));
}

/* stable sort of entry dicts by task_index (KeyError when absent / non-dict). */
static json_t *td_sort_entries(json_t *results)
{
    size_t n = json_len(results);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(results, i);
        if (!td_is_plain_dict(e) || !json_obj_get(e, "task_index"))
            return td_raise_keyerr("task_index");
    }
    /* insertion sort (stable) */
    for (size_t i = 1; i < n; i++) {
        json_t *key = json_get(results, i);
        double kv = td_num(td_get(key, "task_index"));
        size_t j = i;
        while (j > 0) {
            json_t *prev = json_get(results, j - 1);
            if (td_num(td_get(prev, "task_index")) <= kv) break;
            j--;
        }
        if (j != i) {
            /* rotate key into position j */
            json_t *saved = json_copy(key);
            for (size_t k = i; k > j; k--) {
                json_t *src = json_copy(json_get(results, k - 1));
                json_array_set(results, k, src);
            }
            json_array_set(results, j, saved);
        }
    }
    return NULL;
}

/* forward decls for real sibling composition */
static json_t *td_release_unstarted_child(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_record_finished_child(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_waiting_on_resume(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_report_child_done(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_run_children_parallel(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_execute_and_aggregate(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_run_sync_with_note(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_resolve_async_wake_sid(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_resolve_async_session_key(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_units_of(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_dispatch_unit(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_restore_parent_cancellation(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_refuse_detached_only(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_dispatched_payload(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_run_child(td_io_t *io, const json_t *in, json_t *st);
static json_t *td_owner_kwargs(td_io_t *io, const json_t *in, json_t *st);

/* sibling call helpers: build a private in/st for a ported sibling so its
 * carriers resolve, run it, and merge any state writes back. */
static json_t *td_batch_of(json_t *st) { return td_get(st, "batch"); }

/* =====================================================================
 * _Batch.owner_kwargs
 * ===================================================================== */
/* PoP: td_owner_kwargs @ tools/delegate_tool_dispatch.py:owner_kwargs */
static json_t *td_owner_kwargs(td_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    json_t *b = td_batch_of(st);
    json_t *out = json_object();
    json_t *ui = td_get(b, "origin_ui_session_id");
    /* self.origin_ui_session_id or None */
    json_set(out, "owner_session_id", td_truth(ui) ? json_copy(ui) : json_null());
    json_set(out, "owner_transport", json_copy(td_get(b, "origin_owner_transport")));
    json_set(out, "owner_session_record", json_copy(td_get(b, "origin_owner_session_record")));
    return out;
}

/* =====================================================================
 * _Batch.run_child
 * ===================================================================== */
/* PoP: td_run_child @ tools/delegate_tool_dispatch.py:run_child */
static json_t *td_run_child(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *b = td_batch_of(st);
    json_t *task = td_get(in, "task");
    json_t *goal = td_get(task, "goal");
    if (!goal) return td_raise_keyerr("goal");   /* task["goal"] before the seam */
    json_t *child = td_get(st, "child");
    json_t *a = td_args();
    td_arg_n(a, "task_index", td_num(td_get(in, "i")));
    td_arg_j(a, "goal", goal);
    td_arg_j(a, "child", child);
    td_arg_j(a, "parent_agent", td_get(b, "parent_agent"));
    /* **self.owner_kwargs() (owner_session_id = origin_ui_session_id or None) */
    json_t *ui = td_get(b, "origin_ui_session_id");
    td_arg_j(a, "owner_session_id", td_truth(ui) ? ui : NULL);
    td_arg_j(a, "owner_transport", td_get(b, "origin_owner_transport"));
    td_arg_j(a, "owner_session_record", td_get(b, "origin_owner_session_record"));
    return td_call(io, "run_single_child", a);
}

/* =====================================================================
 * _announce_batch
 * ===================================================================== */
/* PoP: td_announce_batch @ tools/delegate_tool_dispatch.py:_announce_batch */
static json_t *td_announce_batch(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = td_get(st, "parent_agent");
    double n_tasks = td_num(td_get(in, "n_tasks"));
    json_t *deleg = td_get(in, "live_deleg_id");
    if (!(n_tasks > 1 && td_truth(deleg))) return json_null();
    json_t *a = td_args();
    td_arg_j(a, "deleg_id", deleg);
    td_arg_j(a, "parent_agent", agent);
    json_t *tag = td_call(io, "format_batch_tag", a);
    if (td_is_raise(tag)) return tag;
    const char *tags = td_str(tag);
    char *hdr = td_xfmt("  " TD_SHUFFLE " [%s] delegating %.0f tasks", tags ? tags : "", n_tasks);
    json_free(tag);
    json_t *spinner = td_getattr(agent, "_delegate_spinner", NULL);
    json_t *pa = td_args();
    td_arg_j(pa, "parent_agent", agent);
    td_arg_j(pa, "spinner", spinner);
    td_arg_s(pa, "line", hdr ? hdr : "");
    td_arg_s(pa, "console_line", hdr ? hdr : "");
    free(hdr);
    json_t *r = td_call(io, "print_completion_line", pa);
    if (td_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* =====================================================================
 * _capture_origin
 * ===================================================================== */
/* PoP: td_capture_origin @ tools/delegate_tool_dispatch.py:_capture_origin */
static json_t *td_capture_origin(td_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *wake = td_call(io, "current_origin_session_id", td_args());
    if (td_is_raise(wake)) return wake;
    json_t *ui = json_string("");
    int delivery = 0;
    /* with _quiet(None): get_session_env then session_history_delivery_supported */
    json_t *a = td_args();
    td_arg_s(a, "key", "HERMES_UI_SESSION_ID");
    td_arg_s(a, "default", "");
    json_t *env = td_call(io, "get_session_env", a);
    if (td_is_raise(env)) {
        json_t *sw = td_quiet_swallow(io, NULL, env, 0);
        if (sw) { json_free(wake); json_free(ui); return sw; }
    } else {
        json_free(ui);
        ui = env;   /* assignment lands only on success */
        json_t *sup = td_call(io, "session_history_delivery_supported", td_args());
        if (td_is_raise(sup)) {
            json_t *sw = td_quiet_swallow(io, NULL, sup, 0);
            if (sw) { json_free(wake); json_free(ui); return sw; }
        } else {
            delivery = td_truth(sup) ? 1 : 0;
            json_free(sup);
        }
    }
    /* *_capture_gateway_steer_authority(_origin_ui_session_id) */
    json_t *sa = td_args();
    td_arg_s(sa, "ui_session_id", td_str(ui) ? td_str(ui) : "");
    json_t *steer = td_call(io, "capture_gateway_steer_authority", sa);
    if (td_is_raise(steer)) { json_free(wake); json_free(ui); return steer; }
    if (!td_is_arr(steer) || json_len(steer) != 2) {
        json_free(steer); json_free(wake); json_free(ui);
        return td_mkraise("ValueError", "not enough values to unpack (expected 2)");
    }
    json_t *out = json_array();
    json_append(out, wake);
    json_append(out, ui);
    json_append(out, json_copy(json_get(steer, 0)));
    json_append(out, json_copy(json_get(steer, 1)));
    json_append(out, json_bool(delivery ? true : false));
    json_free(steer);
    return out;
}

/* =====================================================================
 * _report_child_done
 * ===================================================================== */
/* PoP: td_report_child_done @ tools/delegate_tool_dispatch.py:_report_child_done */
static json_t *td_report_child_done(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = td_get(st, "parent_agent");
    json_t *spinner = td_get(st, "spinner");
    json_t *entry = td_get(in, "entry");
    const char *tag = td_str(td_get(in, "tag"));
    if (!tag) tag = "";
    json_t *labels = td_get(in, "task_labels");
    double n_tasks = td_num(td_get(in, "n_tasks"));
    double remaining = td_num(td_get(in, "remaining"));

    json_t *idxn = td_get(entry, "task_index");
    if (!idxn) return td_raise_keyerr("task_index");   /* entry["task_index"] */
    double idx = td_num(idxn);
    /* label = task_labels[idx] if idx < len else f"Task {idx}" */
    char *label = NULL;
    if (idx < (double)json_len(labels)) {
        json_t *lv = td_list_get(labels, idx);
        if (!lv) return td_mkraise("IndexError", "list index out of range");
        label = td_pystr(lv);
    } else {
        label = td_xfmt("Task %.0f", idx);
    }
    const char *status = td_str(td_get(entry, "status"));
    if (!status) status = "?";
    char *slot;
    if (tag[0]) slot = td_xfmt("%s " TD_MIDDOT " %.0f/%.0f", tag, idx + 1, n_tasks);
    else slot = td_xfmt("%.0f/%.0f", idx + 1, n_tasks);
    /* schema_valid is False and status == "completed" */
    json_t *sv = td_get(entry, "schema_valid");
    int schema_invalid = (sv && sv->type == JSON_BOOL && !sv->bool_val && strcmp(status, "completed") == 0);
    const char *icon = schema_invalid ? TD_WARN : (strcmp(status, "completed") == 0 ? TD_CHECK : TD_XMARK);
    json_t *dur = td_get(entry, "duration_seconds");
    char *durs = td_pystr(dur ? dur : json_number(0));
    char *line = td_xfmt("%s [%s] %s  (%ss)", icon, slot, label ? label : "", durs ? durs : "0");
    free(slot); free(label); free(durs);
    /* _err_line */
    char *errline = NULL;
    int failed_status = (strcmp(status, "failed") == 0 || strcmp(status, "error") == 0 || strcmp(status, "timeout") == 0);
    if (failed_status) {
        json_t *da = td_args();
        td_arg_j(da, "failure_reason", td_get(entry, "failure_reason"));
        td_arg_j(da, "error", td_get(entry, "error"));
        td_arg_n(da, "max_chars", 120);
        json_t *desc = td_call(io, "describe_subagent_failure", da);
        if (td_is_raise(desc)) { free(line); return desc; }
        errline = td_pystr(desc);
        json_free(desc);
    }
    if (schema_invalid) {
        free(errline);
        errline = td_xfmt("output_schema not satisfied " TD_EMDASH " raw text returned (schema_valid=false)");
    }
    if (errline && errline[0]) {
        char *nl = td_xfmt("%s " TD_EMDASH " %s", line, errline);
        free(line); line = nl;
    }
    free(errline);
    json_t *pa = td_args();
    td_arg_j(pa, "parent_agent", agent);
    td_arg_j(pa, "spinner", spinner);
    td_arg_s(pa, "line", line);
    free(line);
    json_t *pr = td_call(io, "print_completion_line", pa);
    if (td_is_raise(pr)) return pr;
    json_free(pr);
    /* if spinner_ref and remaining > 0: with _quiet(...): spinner.update_text(...) */
    if (td_truth(spinner) && remaining > 0) {
        char *full;
        if (tag[0]) {
            char *pfx = td_xfmt("[%s] ", tag);
            full = td_xfmt(TD_SHUFFLE " %s%.0f task%s remaining", pfx, remaining, remaining != 1 ? "s" : "");
            free(pfx);
        } else {
            full = td_xfmt(TD_SHUFFLE " %.0f task%s remaining", remaining, remaining != 1 ? "s" : "");
        }
        json_t *ua = td_args();
        td_arg_s(ua, "text", full);
        free(full);
        json_t *ur = td_call(io, "spinner.update_text", ua);
        if (td_is_raise(ur)) {
            json_t *sw = td_quiet_swallow(io, "Spinner update_text failed: %s", ur, 0);
            if (sw) return sw;
        } else json_free(ur);
    }
    return json_null();
}

/* =====================================================================
 * _record_finished_child
 * ===================================================================== */
/* PoP: td_record_finished_child @ tools/delegate_tool_dispatch.py:_record_finished_child */
static json_t *td_record_finished_child(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *b = td_batch_of(st);
    json_t *entry = td_get(in, "entry");
    int honor = td_truth(td_get(in, "honor_parent_interrupt"));
    json_t *uid = td_get(b, "unit_id");
    if (!honor && td_truth(uid) && td_is_plain_dict(entry)) {
        json_t *a = td_args();
        td_arg_j(a, "unit_id", uid);
        td_arg_j(a, "entry", entry);
        return td_call(io, "record_unit_child", a);
    }
    return json_null();
}

/* =====================================================================
 * _waiting_on_resume
 * ===================================================================== */
/* PoP: td_waiting_on_resume @ tools/delegate_tool_dispatch.py:_waiting_on_resume */
static json_t *td_waiting_on_resume(td_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *a = td_args();
    td_arg_j(a, "entry", td_get(in, "entry"));
    return td_call(io, "waiting_on_resume", a);
}

/* =====================================================================
 * _release_unstarted_child
 * ===================================================================== */
/* PoP: td_release_unstarted_child @ tools/delegate_tool_dispatch.py:_release_unstarted_child */
static json_t *td_release_unstarted_child(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *b = td_batch_of(st);
    json_t *child = td_get(st, "child");
    json_t *ra = td_args();
    td_arg_j(ra, "child", child);
    json_t *r = td_call(io, "release_unstarted", ra);
    if (td_is_raise(r)) return r;
    json_free(r);
    json_t *da = td_args();
    td_arg_j(da, "parent_agent", td_get(b, "parent_agent"));
    td_arg_j(da, "child", child);
    r = td_call(io, "detach_child", da);
    if (td_is_raise(r)) return r;
    json_free(r);
    json_t *ca = td_args();
    td_arg_j(ca, "child", child);
    td_arg_s(ca, "message", "Failed to close a subagent that never started");
    r = td_call(io, "close_child", ca);
    if (td_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* =====================================================================
 * _run_gated
 * ===================================================================== */
/* PoP: td_run_gated @ tools/delegate_tool_dispatch.py:_run_gated */
static json_t *td_run_gated(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *b = td_batch_of(st);
    json_t *gate = td_get(st, "gate");
    (void)gate;   /* gate.run is a seam; the gate carrier is only echoed */
    json_t *child = td_get(st, "child");
    int honor = td_truth(td_get(in, "honor_parent_interrupt"));
    json_t *parent = honor ? td_get(b, "parent_agent") : NULL;
    json_t *ga = td_args();
    td_arg_j(ga, "child", child);
    json_t *fn = json_object();
    json_set(fn, "__callable__", json_bool(true));
    json_set(ga, "fn", fn);
    td_arg_j(ga, "parent", parent);
    json_t *entry = td_call(io, "gate.run", ga);
    if (td_is_raise(entry)) return entry;
    if (!td_is_null(entry)) return entry;   /* entry is not None */
    json_free(entry);
    /* _release_unstarted_child(batch, child) — real sibling */
    json_t *rel = td_release_unstarted_child(io, in, st);
    if (td_is_raise(rel)) return rel;
    json_free(rel);
    json_t *fa = td_args();
    td_arg_n(fa, "idx", td_num(td_get(in, "i")));
    td_arg_s(fa, "status", "interrupted");
    td_arg_s(fa, "error", "Stopped while waiting for the batch's shared prompt cache");
    td_arg_j(fa, "child", child);
    return td_call(io, "fabricated_entry", fa);
}

/* ── sibling-call scaffolding ──
 * Ported siblings compose for REAL. Each runs against a PRIVATE in/st (a copy
 * of the batch carrier) so its reads resolve and its writes never pollute the
 * caller's echoed state window. Read-only siblings get a copied batch; the
 * background dispatcher syncs child unit-id writes back to the origin batch. */
static json_t *td_child_by_index(const json_t *batch, double idx)
{
    json_t *children = td_get(batch, "children");
    if (!td_is_arr(children)) return NULL;
    for (size_t i = 0; i < json_len(children); i++) {
        json_t *tr = json_get(children, i);
        if (td_num(json_get(tr, 0)) == idx) return json_get(tr, 2);
    }
    return NULL;
}

static json_t *td_goal_labels(const json_t *batch)
{
    json_t *tl = td_get(batch, "task_list");
    json_t *out = json_array();
    if (!td_is_arr(tl)) return out;
    for (size_t i = 0; i < json_len(tl); i++) {
        json_t *t = json_get(tl, i);
        json_t *g = td_get(t, "goal");
        if (!g) { json_free(out); return td_raise_keyerr("goal"); }
        const char *gs = td_str(g);
        size_t n = gs ? strlen(gs) : 0;
        if (n > 40) n = 40;
        json_append(out, td_strn(gs ? gs : "", n));
    }
    return out;
}

/* _entry_of(future_id, idx): done? result : fabricated interrupted; a result
 * Exception becomes a fabricated error (BaseException propagates). */
static json_t *td_entry_of(td_io_t *io, const json_t *batch, const char *fid, double idx)
{
    json_t *da = td_args();
    td_arg_s(da, "id", fid);
    json_t *done = td_call(io, "future.done", da);
    if (td_is_raise(done)) return done;
    int is_done = td_truth(done);
    json_free(done);
    json_t *child = td_child_by_index(batch, idx);
    if (!is_done) {
        json_t *fa = td_args();
        td_arg_n(fa, "idx", idx);
        td_arg_s(fa, "status", "interrupted");
        td_arg_s(fa, "error", "Parent agent interrupted " TD_EMDASH " child did not finish in time");
        td_arg_j(fa, "child", child);
        return td_call(io, "fabricated_entry", fa);
    }
    json_t *ra = td_args();
    td_arg_s(ra, "id", fid);
    json_t *res = td_call(io, "future.result", ra);
    if (td_is_raise(res)) {
        if (!td_exc_is_exception(td_kind(res))) return res;   /* BaseException passes */
        json_t *fa = td_args();
        td_arg_n(fa, "idx", idx);
        td_arg_s(fa, "status", "error");
        td_arg_s(fa, "error", td_raise_msg(res));
        td_arg_j(fa, "child", child);
        json_free(res);
        return td_call(io, "fabricated_entry", fa);
    }
    return res;
}

/* =====================================================================
 * _run_children_parallel
 * ===================================================================== */
/* PoP: td_run_children_parallel @ tools/delegate_tool_dispatch.py:_run_children_parallel */
static json_t *td_run_children_parallel(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *batch = td_batch_of(st);
    json_t *results = td_get(st, "results");
    if (!td_is_arr(results)) { results = json_array(); json_set(st, "results", results); }
    int honor = td_truth(td_get(in, "honor_parent_interrupt"));
    json_t *parent_agent = td_get(batch, "parent_agent");
    double n_tasks = (double)json_len(td_get(batch, "task_list"));
    json_t *labels = td_goal_labels(batch);
    if (td_is_raise(labels)) return labels;
    json_t *spinner = td_getattr(parent_agent, "_delegate_spinner", NULL);
    json_t *ta = td_args();
    td_arg_j(ta, "deleg_id", td_get(batch, "live_deleg_id"));
    td_arg_j(ta, "parent_agent", parent_agent);
    json_t *tag = td_call(io, "format_batch_tag", ta);
    if (td_is_raise(tag)) { json_free(labels); return tag; }
    const char *tags = td_str(tag);
    double n_here = (double)json_len(td_get(batch, "children"));
    json_t *children = td_get(batch, "children");

    json_t *ea = td_args();
    td_arg_n(ea, "max_workers", td_num(td_get(batch, "max_children")));
    json_t *exec = td_call(io, "executor.new", ea);
    if (td_is_raise(exec)) { json_free(tag); json_free(labels); return exec; }
    json_free(exec);

    int interrupted = 0;
    json_t *futures = json_array();   /* [{id, idx}] in submission order */
    json_t *pending = NULL;
    json_t *ret = NULL;
    /* try: */
    json_t *sa = td_args();
    json_t *stagger = td_call(io, "get_prefix_stagger_seconds", sa);
    if (td_is_raise(stagger)) { ret = stagger; goto finally; }
    json_t *ga = td_args();
    json_set(ga, "children", td_triple_proj(children));
    json_append(ga, json_copy(stagger));   /* positional stagger arg */
    json_free(stagger);
    json_t *gate = td_call(io, "gate.for_children", ga);
    if (td_is_raise(gate)) { ret = gate; goto finally; }
    json_free(gate);   /* gate None -> run_child, else partial(_run_gated); both project {"__callable__":true} */
    /* futures = {executor.submit(ctx.run, run, i, t, child): i} */
    for (size_t k = 0; k < json_len(children); k++) {
        json_t *tr = json_get(children, k);
        double i = td_num(json_get(tr, 0));
        json_t *t = json_get(tr, 1);
        json_t *c = json_get(tr, 2);
        json_t *sub = td_args();
        json_t *cr = json_object(); json_set(cr, "__callable__", json_bool(true));
        json_set(sub, "ctx_run", cr);
        json_t *fn = json_object(); json_set(fn, "__callable__", json_bool(true));
        json_set(sub, "fn", fn);
        td_arg_n(sub, "i", i);
        td_arg_j(sub, "task", t);
        td_arg_j(sub, "child", c);
        json_t *spec = td_call(io, "executor.submit", sub);
        if (td_is_raise(spec)) { ret = spec; goto finally; }
        const char *fid = td_str(td_get(spec, "id"));
        json_t *frow = json_object();
        td_arg_s(frow, "id", fid ? fid : "");
        td_arg_n(frow, "idx", i);
        json_append(futures, frow);
        json_free(spec);
    }
    /* pending = set(futures) -> children order (ascending id, see header) */
    pending = json_array();
    for (size_t k = 0; k < json_len(futures); k++)
        json_append(pending, json_copy(td_get(json_get(futures, k), "id")));
    while (json_len(pending) > 0) {
        json_t *ir = td_getattr(parent_agent, "_interrupt_requested", NULL);
        int req_true = (ir && ir->type == JSON_BOOL && ir->bool_val);
        if (honor && req_true) {
            for (size_t k = 0; k < json_len(pending); k++) {
                const char *fid = td_str(json_get(pending, k));
                double idx = 0;
                for (size_t j = 0; j < json_len(futures); j++) {
                    json_t *fr = json_get(futures, j);
                    if (strcmp(td_str(td_get(fr, "id")) ? td_str(td_get(fr, "id")) : "", fid ? fid : "") == 0)
                        { idx = td_num(td_get(fr, "idx")); break; }
                }
                json_t *e = td_entry_of(io, batch, fid, idx);
                if (td_is_raise(e)) { ret = e; goto finally; }
                json_append(results, e);
            }
            interrupted = 1;
            break;
        }
        json_t *wa = td_args();
        /* pending ids sorted for the args projection (live set order) */
        json_t *pids = json_copy(pending);
        json_set(wa, "pending", pids);
        td_arg_n(wa, "timeout", 0.5);
        td_arg_s(wa, "return_when", "FIRST_COMPLETED");
        json_t *wr = td_call(io, "cf_wait", wa);
        if (td_is_raise(wr)) { ret = wr; goto finally; }
        json_t *done_ids = json_get(wr, 0);
        json_t *pend_ids = json_get(wr, 1);
        for (size_t k = 0; k < json_len(done_ids); k++) {
            const char *fid = td_str(json_get(done_ids, k));
            double idx = 0;
            for (size_t j = 0; j < json_len(futures); j++) {
                json_t *fr = json_get(futures, j);
                if (strcmp(td_str(td_get(fr, "id")) ? td_str(td_get(fr, "id")) : "", fid ? fid : "") == 0)
                    { idx = td_num(td_get(fr, "idx")); break; }
            }
            json_t *entry = td_entry_of(io, batch, fid, idx);
            if (td_is_raise(entry)) { ret = entry; json_free(wr); goto finally; }
            json_append(results, entry);
            /* _record_finished_child(batch, entry, honor) — real sibling */
            {
                json_t *sin = json_object();
                td_arg_j(sin, "entry", entry);
                td_arg_b(sin, "honor_parent_interrupt", honor);
                json_t *sst = json_object();
                json_set(sst, "batch", json_copy(batch));
                json_t *rr = td_record_finished_child(io, sin, sst);
                json_free(sin); json_free(sst);
                if (td_is_raise(rr)) { ret = rr; json_free(wr); goto finally; }
                json_free(rr);
            }
            /* _report_child_done(...) — real sibling */
            {
                json_t *sin = json_object();
                td_arg_j(sin, "entry", entry);
                td_arg_s(sin, "tag", tags ? tags : "");
                json_set(sin, "task_labels", json_copy(labels));
                td_arg_n(sin, "n_tasks", n_tasks);
                td_arg_n(sin, "remaining", n_here - (double)json_len(results));
                json_t *sst = json_object();
                json_set(sst, "batch", json_copy(batch));
                json_set(sst, "parent_agent", json_copy(parent_agent));
                json_set(sst, "spinner", spinner ? json_copy(spinner) : json_null());
                json_t *rr = td_report_child_done(io, sin, sst);
                json_free(sin); json_free(sst);
                if (td_is_raise(rr)) { ret = rr; json_free(wr); goto finally; }
                json_free(rr);
            }
            /* detached-unit early failure notice */
            const char *estatus = td_str(td_get(entry, "status"));
            int fail_status = estatus && (strcmp(estatus, "failed") == 0 || strcmp(estatus, "error") == 0 || strcmp(estatus, "timeout") == 0);
            json_t *uid = td_get(batch, "unit_id");
            if (!honor && td_truth(uid) && fail_status && (double)json_len(results) < n_here) {
                json_t *sin = json_object();
                td_arg_j(sin, "entry", entry);
                json_t *sst = json_object();
                json_t *wor = td_waiting_on_resume(io, sin, sst);
                json_free(sin); json_free(sst);
                if (td_is_raise(wor)) { ret = wor; json_free(wr); goto finally; }
                int waiting = td_truth(wor);
                json_free(wor);
                if (!waiting) {
                    /* with _quiet("task failure notice failed", exc_info=True): push */
                    double _i = td_num(td_get(entry, "task_index"));
                    int has_i = td_is_int(td_get(entry, "task_index"));
                    json_t *paths = td_get(batch, "live_paths");
                    json_t *_live = NULL;
                    if (has_i && _i >= 0 && _i < (double)json_len(paths)) _live = td_list_get(paths, _i);
                    json_t *payload = json_copy(entry);
                    if (td_truth(_live)) json_set(payload, "live_transcript", json_copy(_live));
                    json_t *pa = td_args();
                    td_arg_j(pa, "unit_id", uid);
                    json_set(pa, "payload", payload);
                    td_arg_n(pa, "n_tasks", n_tasks);
                    json_t *pr = td_call(io, "push_task_failure_notice", pa);
                    if (td_is_raise(pr)) {
                        json_t *sw = td_quiet_swallow(io, "task failure notice failed", pr, 1);
                        if (sw) { ret = sw; json_free(wr); goto finally; }
                    } else json_free(pr);
                }
            }
        }
        /* pending = pend_ids */
        json_t *np = json_array();
        for (size_t k = 0; k < json_len(pend_ids); k++) json_append(np, json_copy(json_get(pend_ids, k)));
        json_free(pending);
        pending = np;
        json_free(wr);
    }
finally:
    json_free(pending);
    /* executor.shutdown(wait=not interrupted, cancel_futures=interrupted) */
    {
        json_t *sha = td_args();
        td_arg_b(sha, "wait", !interrupted);
        td_arg_b(sha, "cancel_futures", interrupted);
        json_t *shr = td_call(io, "executor.shutdown", sha);
        if (td_is_raise(shr)) { if (!ret) ret = shr; else json_free(shr); }
        else json_free(shr);
    }
    if (interrupted && !ret) {
        for (size_t k = 0; k < json_len(futures); k++) {
            json_t *fr = json_get(futures, k);
            const char *fid = td_str(td_get(fr, "id"));
            double idx = td_num(td_get(fr, "idx"));
            json_t *ca = td_args();
            td_arg_s(ca, "id", fid);
            json_t *canc = td_call(io, "future.cancelled", ca);
            if (td_is_raise(canc)) { ret = canc; break; }
            int is_canc = td_truth(canc);
            json_free(canc);
            if (is_canc) {
                json_t *child = td_child_by_index(batch, idx);
                json_t *sin = json_object();
                json_t *sst = json_object();
                json_set(sst, "batch", json_copy(batch));
                json_set(sst, "child", child ? json_copy(child) : json_null());
                json_t *rr = td_release_unstarted_child(io, sin, sst);
                json_free(sin); json_free(sst);
                if (td_is_raise(rr)) { ret = rr; break; }
                json_free(rr);
            }
        }
    }
    json_free(futures);
    json_free(tag);
    json_free(labels);
    if (ret) return ret;
    /* results.sort(key=lambda r: r["task_index"]) */
    json_t *serr = td_sort_entries(results);
    if (serr) return serr;
    return json_null();
}

/* =====================================================================
 * _execute_and_aggregate
 * ===================================================================== */
/* PoP: td_execute_and_aggregate @ tools/delegate_tool_dispatch.py:_execute_and_aggregate */
static json_t *td_execute_and_aggregate(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *batch = td_batch_of(st);
    int honor = td_truth(td_get(in, "honor_parent_interrupt"));
    json_t *children = td_get(batch, "children");
    json_t *results = json_array();
    if (json_len(children) == 1) {
        json_t *tr = json_get(children, 0);
        json_t *sin = json_object();
        td_arg_n(sin, "i", td_num(json_get(tr, 0)));
        td_arg_j(sin, "task", json_get(tr, 1));
        json_t *sst = json_object();
        json_set(sst, "batch", json_copy(batch));
        json_set(sst, "child", json_copy(json_get(tr, 2)));
        json_t *entry = td_run_child(io, sin, sst);
        json_free(sin); json_free(sst);
        if (td_is_raise(entry)) { json_free(results); return entry; }
        json_append(results, entry);
        /* _record_finished_child(batch, results[-1], honor) */
        json_t *rin = json_object();
        td_arg_j(rin, "entry", entry);
        td_arg_b(rin, "honor_parent_interrupt", honor);
        json_t *rst = json_object();
        json_set(rst, "batch", json_copy(batch));
        json_t *rr = td_record_finished_child(io, rin, rst);
        json_free(rin); json_free(rst);
        if (td_is_raise(rr)) { json_free(results); return rr; }
        json_free(rr);
    } else {
        json_t *sin = json_object();
        td_arg_b(sin, "honor_parent_interrupt", honor);
        json_t *sst = json_object();
        json_set(sst, "batch", json_copy(batch));
        json_set(sst, "results", json_array());
        json_t *rr = td_run_children_parallel(io, sin, sst);
        json_free(sin);
        if (td_is_raise(rr)) { json_free(sst); json_free(results); return rr; }
        json_free(rr);
        json_free(results);
        results = json_copy(td_get(sst, "results"));
        json_free(sst);
    }
    /* stamp subagent_id from children_by_index */
    for (size_t k = 0; k < json_len(results); k++) {
        json_t *entry = json_get(results, k);
        json_t *tidx = td_get(entry, "task_index");
        json_t *child = tidx ? td_child_by_index(batch, td_num(tidx)) : NULL;
        json_t *sid = td_getattr(child, "_subagent_id", NULL);
        if (td_is_str(sid) && td_str(sid)[0]) json_set(entry, "subagent_id", json_copy(sid));
    }
    /* _finalize_child_results(results, task_list, children, parent_agent) */
    {
        json_t *fa = td_args();
        json_set(fa, "results", json_copy(results));
        td_arg_j(fa, "task_list", td_get(batch, "task_list"));
        json_set(fa, "children", td_triple_proj(children));
        td_arg_j(fa, "parent_agent", td_get(batch, "parent_agent"));
        json_t *fr = td_call(io, "finalize_child_results", fa);
        if (td_is_raise(fr)) { json_free(results); return fr; }
        json_free(fr);
    }
    /* total_duration = round(time.monotonic() - overall_start, 2) */
    json_t *mono = td_call(io, "time.monotonic", td_args());
    if (td_is_raise(mono)) { json_free(results); return mono; }
    double total_duration = td_round2(td_num(mono) - td_num(td_get(batch, "overall_start")));
    json_free(mono);
    /* live transcript finalize */
    json_t *writers = td_get(batch, "live_writers");
    json_t *paths = td_get(batch, "live_paths");
    for (size_t k = 0; k < json_len(results); k++) {
        json_t *entry = json_get(results, k);
        json_t *tidx = td_get(entry, "task_index");
        double _idx = tidx ? td_num(tidx) : -1;
        int is_int = tidx ? td_is_int(tidx) : 0;
        if (is_int && _idx >= 0 && _idx < (double)json_len(writers)) {
            json_t *w = td_list_get(writers, _idx);
            if (!td_is_null(w)) {
                json_t *wa = td_args();
                td_arg_n(wa, "index", _idx);
                td_arg_j(wa, "entry", entry);
                json_t *wr = td_call(io, "writer.finalize", wa);
                if (td_is_raise(wr)) {
                    json_t *sw = td_quiet_swallow(io, "Live transcript finalize failed", wr, 1);
                    if (sw) { json_free(results); return sw; }
                } else json_free(wr);
                if (_idx < (double)json_len(paths))
                    json_set(entry, "live_transcript", json_copy(td_list_get(paths, _idx)));
            }
        }
    }
    /* refused */
    json_t *refused = td_get(batch, "refused");
    if (td_truth(refused)) {
        for (size_t k = 0; k < json_len(refused); k++) json_append(results, json_copy(json_get(refused, k)));
        json_t *serr = td_sort_entries(results);
        if (serr) { json_free(results); return serr; }
    }
    /* update_manifest_statuses(live_deleg_id, results, home=live_home) */
    {
        json_t *ma = td_args();
        td_arg_j(ma, "deleg_id", td_get(batch, "live_deleg_id"));
        json_set(ma, "results", json_copy(results));
        td_arg_j(ma, "home", td_get(batch, "live_home"));
        json_t *mr = td_call(io, "update_manifest_statuses", ma);
        if (td_is_raise(mr)) { json_free(results); return mr; }
        json_free(mr);
    }
    /* finish_delegation_unit(task_list, results, background=not honor) */
    {
        json_t *fu = td_args();
        td_arg_j(fu, "task_list", td_get(batch, "task_list"));
        json_set(fu, "results", json_copy(results));
        td_arg_b(fu, "background", !honor);
        json_t *fr = td_call(io, "finish_delegation_unit", fu);
        if (td_is_raise(fr)) { json_free(results); return fr; }
        json_free(fr);
    }
    json_t *combined = json_object();
    json_set(combined, "results", results);   /* transfer ownership */
    json_set(combined, "total_duration_seconds", json_number(total_duration));
    json_t *deleg = td_get(batch, "live_deleg_id");
    if (td_truth(deleg)) json_set(combined, "delegation_id", json_copy(deleg));
    /* process_notes */
    json_t *notes = json_array();
    for (size_t k = 0; k < json_len(results); k++) {
        json_t *pa = td_args();
        td_arg_j(pa, "entry", json_get(results, k));
        json_t *pr = td_call(io, "process_accounting_lines", pa);
        if (td_is_raise(pr)) { json_free(notes); json_free(combined); return pr; }
        if (td_is_arr(pr)) for (size_t j = 0; j < json_len(pr); j++) json_append(notes, json_copy(json_get(pr, j)));
        json_free(pr);
    }
    if (json_len(notes) > 0) json_set(combined, "process_notes", notes);
    else json_free(notes);
    /* unit_paths */
    json_t *unit_paths = json_array();
    for (size_t k = 0; k < json_len(children); k++) {
        double i = td_num(json_get(json_get(children, k), 0));
        if (i < (double)json_len(paths)) json_append(unit_paths, json_copy(td_list_get(paths, i)));
    }
    if (json_len(unit_paths) > 0) json_set(combined, "live_transcripts", unit_paths);
    else json_free(unit_paths);
    json_t *group = td_get(batch, "group");
    if (!td_is_null(group)) json_set(combined, "group", json_copy(group));
    return combined;
}

/* =====================================================================
 * _run_sync_with_note
 * ===================================================================== */
/* PoP: td_run_sync_with_note @ tools/delegate_tool_dispatch.py:_run_sync_with_note */
static json_t *td_run_sync_with_note(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *batch = td_batch_of(st);
    json_t *sin = json_object();
    td_arg_b(sin, "honor_parent_interrupt", 1);
    json_t *sst = json_object();
    json_set(sst, "batch", json_copy(batch));
    json_t *result = td_execute_and_aggregate(io, sin, sst);
    json_free(sin); json_free(sst);
    if (td_is_raise(result)) return result;
    const char *reason = td_str(td_get(in, "reason"));
    const char *note = NULL;
    if (reason && strcmp(reason, "no_async") == 0) note = TD_NOTE_NO_ASYNC;
    else if (reason && strcmp(reason, "at_capacity") == 0) note = TD_NOTE_AT_CAPACITY;
    else { json_free(result); return td_raise_keyerr(reason ? reason : ""); }
    if (td_is_plain_dict(result)) json_set(result, "note", json_string(note));
    char *s = td_json_dumps(result);
    json_free(result);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* =====================================================================
 * _resolve_async_wake_sid
 * ===================================================================== */
/* PoP: td_resolve_async_wake_sid @ tools/delegate_tool_dispatch.py:_resolve_async_wake_sid */
static json_t *td_resolve_async_wake_sid(td_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *wake = td_str(td_get(in, "origin_wake_sid"));
    int delivery = td_truth(td_get(in, "origin_session_history_delivery"));
    int oneshot = td_truth(td_get(in, "oneshot_session"));
    json_t *finite = td_call(io, "finite_turn_required", td_args());
    if (td_is_raise(finite)) return finite;
    int finite_true = (finite->type == JSON_BOOL && finite->bool_val);
    int finite_none = td_is_null(finite);
    json_free(finite);
    /* oneshot or finite is True or (finite is None and env == "1") */
    if (oneshot || finite_true) return json_null();
    if (finite_none) {
        json_t *ea = td_args();
        td_arg_s(ea, "key", "HERMES_SINGLE_QUERY_SESSION");
        json_t *env = td_call(io, "get_session_env", ea);
        if (td_is_raise(env)) return env;
        const char *ev = td_str(env);
        int is_one = ev && strcmp(ev, "1") == 0;
        json_free(env);
        if (is_one) return json_null();
    }
    /* try: if async_delivery_supported(): return ""  except Exception: return "" */
    json_t *ad = td_call(io, "async_delivery_supported", td_args());
    if (td_is_raise(ad)) {
        if (td_exc_is_exception(td_kind(ad))) { json_free(ad); return json_string(""); }
        return ad;   /* BaseException passes */
    }
    int supported = td_truth(ad);
    json_free(ad);
    if (supported) return json_string("");
    if (wake && wake[0] && delivery) {
        json_t *la = json_array();
        json_append(la, json_string(wake));
        json_t *lr = td_logger(io, "info",
            "delegate_task: session %s resumes server history " TD_EMDASH " detached result will be persisted "
            "for the next client turn (no model wake).", la, 0);
        if (td_is_raise(lr)) return lr;
        json_free(lr);
        return json_string(wake);
    }
    if (wake && wake[0]) {
        json_t *la = json_array();
        json_append(la, json_string(wake));
        json_t *lr = td_logger(io, "info",
            "delegate_task: session %s has no declared server-history consumer " TD_EMDASH " running the batch "
            "synchronously so the result returns in this turn.", la, 0);
        if (td_is_raise(lr)) return lr;
        json_free(lr);
    }
    return json_null();
}

/* =====================================================================
 * _resolve_async_session_key
 * ===================================================================== */
/* PoP: td_resolve_async_session_key @ tools/delegate_tool_dispatch.py:_resolve_async_session_key */
static json_t *td_resolve_async_session_key(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *agent = td_get(st, "parent_agent");
    const char *origin_ui = td_str(td_get(in, "origin_ui_session_id"));
    if (!origin_ui) origin_ui = "";
    json_t *ka = td_args();
    td_arg_s(ka, "default", "");
    json_t *key = td_call(io, "get_current_session_key", ka);
    if (td_is_raise(key)) return key;
    /* agent_session_id = str(getattr(parent_agent, "session_id", "") or "") */
    json_t *sid = td_getattr(agent, "session_id", NULL);
    char *agsid = td_truth(sid) ? td_pystr(sid) : td_xfmt("");
    char *session_key = td_xfmt("%s", td_str(key) ? td_str(key) : "");
    json_free(key);
    char *cur_ui = td_xfmt("%s", origin_ui);
    /* with _quiet(None): source / ui refresh / tui override */
    json_t *sa = td_args();
    td_arg_s(sa, "key", "HERMES_SESSION_SOURCE");
    td_arg_s(sa, "default", "");
    json_t *src = td_call(io, "get_session_env", sa);
    if (td_is_raise(src)) {
        json_t *sw = td_quiet_swallow(io, NULL, src, 0);
        if (sw) { free(agsid); free(session_key); free(cur_ui); return sw; }
        goto done;
    }
    {
        const char *source = td_str(src);
        json_free(src);
        json_t *ua = td_args();
        td_arg_s(ua, "key", "HERMES_UI_SESSION_ID");
        td_arg_s(ua, "default", "");
        json_t *ui = td_call(io, "get_session_env", ua);
        if (td_is_raise(ui)) {
            json_t *sw = td_quiet_swallow(io, NULL, ui, 0);
            if (sw) { free(agsid); free(session_key); free(cur_ui); return sw; }
            goto done;
        }
        const char *uis = td_str(ui);
        if (uis && uis[0]) { free(cur_ui); cur_ui = td_xfmt("%s", uis); }
        json_free(ui);
        if (source && strcmp(source, "tui") == 0 && agsid[0]) {
            free(session_key);
            session_key = td_xfmt("%s", agsid);
        }
    }
done:
    {
        const char *rk = (session_key && session_key[0]) ? session_key : agsid;
        json_t *out = json_array();
        json_append(out, json_string(rk));
        json_append(out, json_string(cur_ui));
        free(agsid); free(session_key); free(cur_ui);
        return out;
    }
}

/* =====================================================================
 * _batch_progress_token
 * ===================================================================== */
/* PoP: td_batch_progress_token @ tools/delegate_tool_dispatch.py:_batch_progress_token */
static json_t *td_batch_progress_token(td_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *children = td_get(st, "child_agents");
    json_t *parts = json_array();
    int in_tool = 0;
    for (size_t k = 0; k < json_len(children); k++) {
        json_t *ca = td_args();
        td_arg_n(ca, "index", (double)k);
        json_t *sum = td_call(io, "child.get_activity_summary", ca);
        if (td_is_raise(sum)) {
            if (!td_exc_is_exception(td_kind(sum))) { json_free(parts); return sum; }
            json_free(sum);
            json_append(parts, json_null());
            continue;
        }
        if (!td_is_plain_dict(sum)) {
            /* summary.get raises AttributeError -> except -> None */
            json_free(sum);
            json_append(parts, json_null());
            continue;
        }
        json_t *tool = td_get(sum, "current_tool");
        json_t *row = json_array();
        json_t *acc = td_get(sum, "api_call_count");
        json_append(row, acc ? json_copy(acc) : json_number(0));
        json_append(row, tool ? json_copy(tool) : json_null());
        json_t *ts = td_get(sum, "last_activity_ts");
        json_append(row, ts ? json_copy(ts) : json_null());
        json_append(parts, row);
        in_tool = in_tool || td_truth(tool);
        json_free(sum);
    }
    json_t *out = json_array();
    json_append(out, parts);
    json_append(out, json_bool(in_tool ? true : false));
    return out;
}

/* =====================================================================
 * _dispatched_payload
 * ===================================================================== */
/* PoP: td_dispatched_payload @ tools/delegate_tool_dispatch.py:_dispatched_payload */
static json_t *td_dispatched_payload(td_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *batch = td_batch_of(st);
    json_t *units = td_get(in, "dispatched");   /* [[unit, uid], ...] */
    json_t *tl = td_get(batch, "task_list");
    json_t *goals = json_array();
    for (size_t k = 0; k < json_len(tl); k++) {
        json_t *g = td_get(json_get(tl, k), "goal");
        if (!g) { json_free(goals); return td_raise_keyerr("goal"); }
        json_append(goals, json_copy(g));
    }
    json_t *children = td_get(batch, "children");
    double n = (double)json_len(children);
    json_t *deleg = td_get(batch, "live_deleg_id");
    json_t *payload = json_object();
    json_set(payload, "status", json_string("dispatched"));
    json_set(payload, "mode", json_string("background"));
    json_set(payload, "count", json_number(n));
    /* delegation_id = batch.live_deleg_id or units[0][1] */
    if (td_truth(deleg)) json_set(payload, "delegation_id", json_copy(deleg));
    else {
        json_t *u0 = td_list_get(units, 0);
        if (!u0) { json_free(goals); json_free(payload); return td_mkraise("IndexError", "list index out of range"); }
        json_set(payload, "delegation_id", json_copy(json_get(u0, 1)));
    }
    json_set(payload, "goals", goals);
    char *note;
    if (n == 1) note = td_xfmt("%s", TD_NOTE_ONE);
    else {
        char *ns = td_xfmt("%.0f", n);
        char *ks = td_xfmt("%.0f", (double)json_len(units));
        note = td_xfmt(TD_NOTE_MANY, ns, ks);
        free(ns); free(ks);
    }
    json_set(payload, "note", json_string(note));
    free(note);
    if (json_len(units) > 1) {
        json_t *uarr = json_array();
        for (size_t k = 0; k < json_len(units); k++) {
            json_t *pair = json_get(units, k);
            json_t *unit = json_get(pair, 0);
            json_t *uid = json_get(pair, 1);
            json_t *uo = json_object();
            td_arg_j(uo, "delegation_id", uid);
            td_arg_j(uo, "group", td_get(unit, "group"));
            json_t *tidx = json_array();
            json_t *uch = td_get(unit, "children");
            for (size_t j = 0; j < json_len(uch); j++) json_append(tidx, json_copy(json_get(json_get(uch, j), 0)));
            json_set(uo, "task_indexes", tidx);
            json_append(uarr, uo);
        }
        json_set(payload, "units", uarr);
    }
    /* sids */
    json_t *sids = json_array();
    int any_sid = 0;
    for (size_t k = 0; k < json_len(children); k++) {
        json_t *c = json_get(json_get(children, k), 2);
        json_t *sid = td_getattr(c, "_subagent_id", NULL);
        json_append(sids, sid ? json_copy(sid) : json_null());
        if (td_is_str(sid) && td_str(sid)[0]) any_sid = 1;
    }
    if (any_sid) {
        json_set(payload, "subagent_ids", sids);
        json_set(payload, "control_hint", json_string(TD_NOTE_CONTROL_HINT));
    } else json_free(sids);
    json_t *refused = td_get(batch, "refused");
    if (td_truth(refused)) json_set(payload, "refused", json_copy(refused));
    json_t *paths = td_get(batch, "live_paths");
    if (td_truth(paths)) {
        json_set(payload, "live_transcripts", json_copy(paths));
        json_set(payload, "live_transcripts_hint", json_string(TD_NOTE_LIVE_TRANSCRIPTS_HINT));
    }
    return payload;
}

/* =====================================================================
 * _units_of
 * ===================================================================== */
/* PoP: td_units_of @ tools/delegate_tool_dispatch.py:_units_of */
static json_t *td_units_of(td_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *batch = td_batch_of(st);
    json_t *ic = td_call(io, "get_independent_completions", td_args());
    if (td_is_raise(ic)) return ic;
    int independent = td_truth(ic);
    json_free(ic);
    json_t *out = json_array();
    if (!independent) {
        json_append(out, json_copy(batch));
        return out;
    }
    /* members keyed by ("g", str(g)) or ("i", i), first-appearance order */
    json_t *children = td_get(batch, "children");
    json_t *keys = json_array();    /* parallel arrays: key tag, key val, members */
    json_t *vals = json_array();
    json_t *members = json_array();
    for (size_t k = 0; k < json_len(children); k++) {
        json_t *tr = json_get(children, k);
        double i = td_num(json_get(tr, 0));
        json_t *t = json_get(tr, 1);
        json_t *g = td_get(t, "group");
        int grouped = !(td_is_null(g) || (td_is_str(g) && td_str(g)[0] == '\0'));
        char *tag; char *val;
        if (grouped) { tag = td_xfmt("g"); val = td_pystr(g); }
        else { tag = td_xfmt("i"); val = td_xfmt("%.0f", i); }
        /* find existing key */
        long long found = -1;
        for (size_t j = 0; j < json_len(keys); j++) {
            if (strcmp(td_str(json_get(keys, j)), tag) == 0 && strcmp(td_str(json_get(vals, j)), val) == 0)
                { found = (long long)j; break; }
        }
        if (found < 0) {
            json_append(keys, json_string(tag));
            json_append(vals, json_string(val));
            json_t *m = json_array();
            json_append(m, json_copy(tr));
            json_append(members, m);
        } else {
            json_append(json_get(members, (size_t)found), json_copy(tr));
        }
        free(tag); free(val);
    }
    for (size_t j = 0; j < json_len(keys); j++) {
        json_t *unit = json_copy(batch);
        json_set(unit, "children", json_copy(json_get(members, j)));
        const char *tag = td_str(json_get(keys, j));
        if (strcmp(tag, "g") == 0) json_set(unit, "group", json_copy(json_get(vals, j)));
        else json_set(unit, "group", json_null());
        json_append(out, unit);
    }
    json_free(keys); json_free(vals); json_free(members);
    return out;
}

/* =====================================================================
 * _dispatch_unit
 * ===================================================================== */
/* PoP: td_dispatch_unit @ tools/delegate_tool_dispatch.py:_dispatch_unit */
static json_t *td_dispatch_unit(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *unit = td_get(st, "unit");
    json_t *children = td_get(unit, "children");
    json_t *tl = td_get(unit, "task_list");
    /* goals */
    json_t *goals = json_array();
    for (size_t k = 0; k < json_len(tl); k++) {
        json_t *g = td_get(json_get(tl, k), "goal");
        if (!g) { json_free(goals); return td_raise_keyerr("goal"); }
        json_append(goals, json_copy(g));
    }
    /* model = creds["model"] or next(child model truthy, None) */
    json_t *creds = td_get(unit, "creds");
    json_t *cm = td_get(creds, "model");
    if (!cm) { json_free(goals); return td_raise_keyerr("model"); }
    json_t *model = NULL;
    if (td_truth(cm)) model = json_copy(cm);
    else {
        for (size_t k = 0; k < json_len(children); k++) {
            json_t *c = json_get(json_get(children, k), 2);
            json_t *m = td_getattr(c, "model", NULL);
            if (td_truth(m)) { model = json_copy(m); break; }
        }
    }
    /* task_indexes */
    json_t *tidx = NULL;
    if (json_len(children) < json_len(tl)) {
        tidx = json_array();
        for (size_t k = 0; k < json_len(children); k++) json_append(tidx, json_copy(json_get(json_get(children, k), 0)));
    }
    /* task_transcripts */
    json_t *tt = json_object();
    json_t *writers = td_get(unit, "live_writers");
    for (size_t k = 0; k < json_len(children); k++) {
        double i = td_num(json_get(json_get(children, k), 0));
        if (i < (double)json_len(writers)) {
            json_t *w = td_list_get(writers, i);
            if (!td_is_null(w)) {
                json_t *path = td_dattr(w, "path");
                if (!path) { json_free(goals); json_free(model); json_free(tidx); json_free(tt); return td_raise_attr(w, "path"); }
                if (!td_is_null(path)) {
                    char *ks = td_xfmt("%.0f", i);
                    char *vs = td_pystr(path);
                    json_set(tt, ks, json_string(vs));
                    free(ks); free(vs);
                }
            }
        }
    }
    json_t *a = td_args();
    json_set(a, "goals", goals);
    td_arg_j(a, "context", td_get(unit, "context"));
    /* toolsets=None dropped */
    td_arg_j(a, "role", td_get(unit, "top_role"));
    td_arg_j(a, "model", model);
    json_free(model);
    json_t *runner = json_object(); json_set(runner, "__callable__", json_bool(true));
    json_set(a, "runner", runner);
    json_t *ifn = json_object(); json_set(ifn, "__callable__", json_bool(true));
    json_set(a, "interrupt_fn", ifn);
    td_arg_j(a, "delegation_id", td_get(in, "unit_id"));
    td_arg_j(a, "slot_key", td_get(in, "slot_key"));
    if (tidx) json_set(a, "task_indexes", tidx);
    json_set(a, "task_transcripts", tt);
    json_t *pfn = json_object(); json_set(pfn, "__callable__", json_bool(true));
    json_set(a, "progress_fn", pfn);
    /* **routing */
    json_t *routing = td_get(in, "routing");
    if (td_is_obj(routing)) {
        for (size_t k = 0; k < routing->c.count; k++) {
            json_t *v = routing->c.items[k];
            if (v && v->type != JSON_NULL) json_set(a, routing->c.keys[k], json_copy(v));
        }
    }
    return td_call(io, "dispatch_async", a);
}

/* =====================================================================
 * _restore_parent_cancellation
 * ===================================================================== */
/* PoP: td_restore_parent_cancellation @ tools/delegate_tool_dispatch.py:_restore_parent_cancellation */
static json_t *td_restore_parent_cancellation(td_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *unit = td_get(st, "unit");
    json_t *children = td_get(unit, "children");
    for (size_t k = 0; k < json_len(children); k++) {
        json_t *c = json_get(json_get(children, k), 2);
        json_t *a = td_args();
        td_arg_j(a, "parent_agent", td_get(unit, "parent_agent"));
        td_arg_j(a, "child", c);
        json_t *r = td_call(io, "attach_child", a);
        if (td_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* =====================================================================
 * _refuse_detached_only
 * ===================================================================== */
/* PoP: td_refuse_detached_only @ tools/delegate_tool_dispatch.py:_refuse_detached_only */
static json_t *td_refuse_detached_only(td_io_t *io, const json_t *in, json_t *st)
{
    json_t *batch = td_batch_of(st);
    json_t *children = td_get(batch, "children");
    for (size_t k = 0; k < json_len(children); k++) {
        json_t *c = json_get(json_get(children, k), 2);
        json_t *sin = json_object();
        json_t *sst = json_object();
        json_set(sst, "batch", json_copy(batch));
        json_set(sst, "child", json_copy(c));
        json_t *rr = td_release_unstarted_child(io, sin, sst);
        json_free(sin); json_free(sst);
        if (td_is_raise(rr)) return rr;
        json_free(rr);
    }
    const char *why = td_str(td_get(in, "why"));
    char *msg = td_xfmt("Not continued: %s.", why ? why : "");
    json_t *a = td_args();
    td_arg_s(a, "message", msg);
    free(msg);
    return td_call(io, "tool_error", a);
}

/* write child._delegation_unit_id on a wrapper (auto-vivify __attr__). */
static void td_set_child_unit_id(json_t *child, const char *uid)
{
    if (!td_is_obj(child)) return;
    json_t *inner = td_get(child, "__attr__");
    if (!inner) { inner = json_object(); json_set(child, "__attr__", inner); }
    json_set(inner, "_delegation_unit_id", json_string(uid ? uid : ""));
}

/* =====================================================================
 * _dispatch_background
 * ===================================================================== */
/* PoP: td_dispatch_background @ tools/delegate_tool_dispatch.py:_dispatch_background */
static json_t *td_dispatch_background(td_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *batch = td_batch_of(st);
    json_t *parent_agent = td_get(batch, "parent_agent");
    /* oneshot_session = getattr(parent_agent, "_oneshot_session", None) is True */
    json_t *os = td_getattr(parent_agent, "_oneshot_session", NULL);
    int oneshot = (os && os->type == JSON_BOOL && os->bool_val);
    json_t *win = json_object();
    td_arg_j(win, "origin_wake_sid", td_get(batch, "origin_wake_sid"));
    td_arg_b(win, "origin_session_history_delivery", td_truth(td_get(batch, "origin_session_history_delivery")));
    td_arg_b(win, "oneshot_session", oneshot);
    json_t *wst = json_object();
    json_t *wake = td_resolve_async_wake_sid(io, win, wst);
    json_free(win); json_free(wst);
    if (td_is_raise(wake)) return wake;
    if (td_is_null(wake)) {
        json_free(wake);
        json_t *lr = td_logger(io, "info",
            "delegate_task: async delivery unsupported on this session runtime; running the batch synchronously instead.",
            json_array(), 0);
        if (td_is_raise(lr)) return lr;
        json_free(lr);
        json_t *sin = json_object(); td_arg_s(sin, "reason", "no_async");
        json_t *sst = json_object(); json_set(sst, "batch", json_copy(batch));
        json_t *r = td_run_sync_with_note(io, sin, sst);
        json_free(sin); json_free(sst);
        return r;
    }
    /* session_key, origin_ui_session_id = _resolve_async_session_key(...) */
    json_t *kin = json_object();
    td_arg_j(kin, "origin_ui_session_id", td_get(batch, "origin_ui_session_id"));
    json_t *kst = json_object(); json_set(kst, "parent_agent", json_copy(parent_agent));
    json_t *keypair = td_resolve_async_session_key(io, kin, kst);
    json_free(kin); json_free(kst);
    if (td_is_raise(keypair)) { json_free(wake); return keypair; }
    /* routing = dict(session_key, origin_ui_session_id, origin_session_id,
     *                 parent_session_id=getattr(...), max_async_children=seam) */
    json_t *routing = json_object();
    td_arg_j(routing, "session_key", json_get(keypair, 0));
    td_arg_j(routing, "origin_ui_session_id", json_get(keypair, 1));
    td_arg_j(routing, "origin_session_id", wake);
    td_arg_j(routing, "parent_session_id", td_getattr(parent_agent, "session_id", NULL));
    json_free(keypair);
    json_t *mac = td_call(io, "get_max_async_children", td_args());
    if (td_is_raise(mac)) { json_free(wake); json_free(routing); return mac; }
    td_arg_j(routing, "max_async_children", mac);
    json_free(mac);
    /* units = [replace(unit, refused=[]) for unit in _units_of(batch)] */
    json_t *uin = json_object();
    json_t *ust = json_object(); json_set(ust, "batch", json_copy(batch));
    json_t *units = td_units_of(io, uin, ust);
    json_free(uin); json_free(ust);
    if (td_is_raise(units)) { json_free(wake); json_free(routing); return units; }
    for (size_t k = 0; k < json_len(units); k++) json_set(json_get(units, k), "refused", json_array());
    json_t *dispatched = json_array();   /* [[unit, uid], ...] */
    json_t *inline_results = json_array();
    char *slot_key = NULL;
    json_t *ret = NULL;
    double nunits = (double)json_len(units);
    json_t *deleg = td_get(batch, "live_deleg_id");
    for (size_t k = 0; k < json_len(units); k++) {
        json_t *unit = json_get(units, k);
        /* unit_id */
        char *unit_id = NULL;
        if (nunits == 1) unit_id = td_xfmt("%s", td_str(deleg) ? td_str(deleg) : "");
        else if (td_truth(deleg)) unit_id = td_xfmt("%s-%zu", td_str(deleg), k + 1);
        if (!unit_id || !unit_id[0]) {
            free(unit_id);
            json_t *nid = td_call(io, "new_delegation_id", td_args());
            if (td_is_raise(nid)) { ret = nid; goto cleanup; }
            unit_id = td_xfmt("%s", td_str(nid) ? td_str(nid) : "");
            json_free(nid);
        }
        json_set(unit, "unit_id", json_string(unit_id));
        /* child._delegation_unit_id = unit_id (write unit child + sync origin batch) */
        json_t *uch = td_get(unit, "children");
        for (size_t j = 0; j < json_len(uch); j++) {
            json_t *tr = json_get(uch, j);
            td_set_child_unit_id(json_get(tr, 2), unit_id);
            json_t *bc = td_child_by_index(batch, td_num(json_get(tr, 0)));
            if (bc) td_set_child_unit_id(bc, unit_id);
        }
        /* detach this unit's children */
        for (size_t j = 0; j < json_len(uch); j++) {
            json_t *c = json_get(json_get(uch, j), 2);
            json_t *da = td_args();
            td_arg_j(da, "parent_agent", parent_agent);
            td_arg_j(da, "child", c);
            json_t *dr = td_call(io, "detach_child", da);
            if (td_is_raise(dr)) { ret = dr; free(unit_id); goto cleanup; }
            json_free(dr);
        }
        /* dispatch = _dispatch_unit(unit, unit_id, slot_key, routing) */
        json_t *din = json_object();
        td_arg_s(din, "unit_id", unit_id);
        td_arg_s(din, "slot_key", slot_key);
        json_set(din, "routing", json_copy(routing));
        json_t *dst = json_object(); json_set(dst, "unit", json_copy(unit));
        json_t *dispatch = td_dispatch_unit(io, din, dst);
        json_free(din); json_free(dst);
        if (td_is_raise(dispatch)) { ret = dispatch; free(unit_id); goto cleanup; }
        const char *dstatus = td_str(td_get(dispatch, "status"));
        if (dstatus && strcmp(dstatus, "dispatched") == 0) {
            json_t *did = td_get(dispatch, "delegation_id");
            if (!did) { ret = td_raise_keyerr("delegation_id"); json_free(dispatch); free(unit_id); goto cleanup; }
            if (!slot_key || !slot_key[0]) { free(slot_key); slot_key = td_xfmt("%s", td_str(did) ? td_str(did) : ""); }
            json_t *pair = json_array();
            json_append(pair, json_copy(unit));
            json_append(pair, json_copy(did));
            json_append(dispatched, pair);
            json_free(dispatch);
            free(unit_id);
            continue;
        }
        /* _restore_parent_cancellation(unit) */
        {
            json_t *rin = json_object();
            json_t *rst = json_object(); json_set(rst, "unit", json_copy(unit));
            json_t *rr = td_restore_parent_cancellation(io, rin, rst);
            json_free(rin); json_free(rst);
            if (td_is_raise(rr)) { ret = rr; json_free(dispatch); free(unit_id); goto cleanup; }
            json_free(rr);
        }
        int none_dispatched = (json_len(dispatched) == 0);
        int detached_only = td_truth(td_get(batch, "detached_only"));
        const char *derr = td_str(td_get(dispatch, "error"));
        if (none_dispatched && detached_only) {
            char *why = td_xfmt("the background pool is at capacity (%s)", derr ? derr : "rejected");
            json_t *sin = json_object(); td_arg_s(sin, "why", why);
            free(why);
            json_t *sst = json_object(); json_set(sst, "batch", json_copy(batch));
            json_t *rr = td_refuse_detached_only(io, sin, sst);
            json_free(sin); json_free(sst);
            ret = rr;
            json_free(dispatch); free(unit_id);
            goto cleanup;
        }
        if (none_dispatched) {
            json_t *la = json_array();
            json_append(la, json_string(derr ? derr : "rejected"));
            json_t *lr = td_logger(io, "info",
                "delegate_task: async pool at capacity (%s); running the whole batch synchronously instead.", la, 0);
            if (td_is_raise(lr)) { ret = lr; json_free(dispatch); free(unit_id); goto cleanup; }
            json_free(lr);
            json_t *sin = json_object(); td_arg_s(sin, "reason", "at_capacity");
            json_t *sst = json_object(); json_set(sst, "batch", json_copy(batch));
            json_t *rr = td_run_sync_with_note(io, sin, sst);
            json_free(sin); json_free(sst);
            ret = rr;
            json_free(dispatch); free(unit_id);
            goto cleanup;
        }
        /* later unit rejected: logger.warning + inline execute */
        {
            json_t *la = json_array();
            json_append(la, json_number((double)(k + 1)));
            json_append(la, json_number(nunits));
            json_append(la, derr ? json_string(derr) : json_null());
            json_t *lr = td_logger(io, "warning",
                "delegate_task: unit %d/%d not accepted (%s); running it inline.", la, 0);
            if (td_is_raise(lr)) { ret = lr; json_free(dispatch); free(unit_id); goto cleanup; }
            json_free(lr);
            json_t *sin = json_object(); td_arg_b(sin, "honor_parent_interrupt", 1);
            json_t *sst = json_object(); json_set(sst, "batch", json_copy(unit));
            json_t *agg = td_execute_and_aggregate(io, sin, sst);
            json_free(sin); json_free(sst);
            if (td_is_raise(agg)) { ret = agg; json_free(dispatch); free(unit_id); goto cleanup; }
            json_t *res = td_get(agg, "results");
            if (!res) { ret = td_raise_keyerr("results"); json_free(agg); json_free(dispatch); free(unit_id); goto cleanup; }
            for (size_t j = 0; j < json_len(res); j++) json_append(inline_results, json_copy(json_get(res, j)));
            json_free(agg);
        }
        json_free(dispatch);
        free(unit_id);
    }
    /* payload = _dispatched_payload(batch, dispatched) */
    {
        json_t *pin = json_object();
        json_set(pin, "dispatched", json_copy(dispatched));
        json_t *pst = json_object(); json_set(pst, "batch", json_copy(batch));
        json_t *payload = td_dispatched_payload(io, pin, pst);
        json_free(pin); json_free(pst);
        if (td_is_raise(payload)) { ret = payload; goto cleanup; }
        if (json_len(inline_results) > 0) json_set(payload, "inline_results", json_copy(inline_results));
        char *s = td_json_dumps(payload);
        json_free(payload);
        ret = json_string(s ? s : "");
        free(s);
    }
cleanup:
    free(slot_key);
    json_free(wake);
    json_free(routing);
    json_free(units);
    json_free(dispatched);
    json_free(inline_results);
    return ret;
}