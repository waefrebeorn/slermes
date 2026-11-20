/* delegate_prefix_gate_flow2.c — FLOW port of the prompt-cache warm-up gate
 * surface in tools/delegate_tool_prefix_gate.py (LIVE authority in the quarry).
 *
 * The module has 9 defs. tool_dispatch_flow2.c already holds PoP claims for 6
 * of them (_route_key, _caches_prompts, _first_response_started,
 * _stop_requested, PrefixCacheGate.for_children, PrefixCacheGate.run), modelling
 * them there as universal-seam value calls inside the dispatch surface. This
 * dedicated oracle re-derives the WHOLE gate surface (all 9 defs) from the live
 * source so the gate lifecycle is differentially checked as one standalone
 * unit: the 3 defs with no claim anywhere (PrefixCacheGate.__init__, .follows,
 * ._wait) are the genuine remainder; the other 6 are re-derived here (the
 * tool_dispatch_flow2 claims remain in force there).
 *
 *   module functions: _route_key, _caches_prompts, _first_response_started,
 *                     _stop_requested
 *   PrefixCacheGate:  __init__, for_children, follows, run, _wait
 *
 * Decision sequences only: Python truthiness, `or` short-circuits, `is True`
 * identity vs truthy tests, `is not` object identity (the __id__ model),
 * tuple route equality, try/finally (the leader_done.set() in run — runs for
 * EVERY exception kind including the BaseException trio), dict insertion order
 * and ordered seam traces. A scripted raise from any seam propagates; no
 * except site exists in this module, so the FULL builtin exception-parent table
 * below is carried for the raise-kind machinery ( audited against py3.14 ).
 *
 * Universal seam (name -> scripted value / raise spec), args borrowed, result
 * caller-owned. Seams used here:
 *   "time.monotonic"  {}                     -> number (monotonic clock)
 *   "event.wait"      {"timeout": seconds}   -> ignored by _wait
 *   "gate.run_fn"     {}                     -> the child's run() callable
 * (the run() callable's seam name is taken from its {"__callable__": name}
 * marker, defaulting to "gate.run_fn"). Fail-closed: an absent / exhausted
 * script returns a RuntimeError raise spec, recorded as the event ret BEFORE
 * propagation; a bare array script is a per-call QUEUE; a list RESULT is
 * wrapped {"__script__":true,"ret":[...]}; a scripted null returns NULL.
 *
 * Value model (shared with tests/oracle_delegate_prefix_gate_flow2.py):
 *   - child/leader/parent are attribute doubles
 *     {"__attr__":{...},"__id__":"c0"?,...} <-> SimpleNamespace; the oracle
 *     interns by __id__ so `child is leader` agrees with dpg2_same_id.
 *   - a gate is {"leader":<double>,"max_wait_s":num,"leader_route":[...],
 *     "leader_done":bool}; _leader_route is a tuple live, a JSON array here
 *     (projected back to an array in the echo — equality is structural).
 *   - leader_done is a state flag: is_set/set are silent, wait rides the
 *     event.wait seam (its return is ignored by _wait).
 *   - callables project to {"__callable__":seam}; raise specs are
 *     {"__raise__":"<Class>","message":str(e)}; integral floats read as ints.
 *
 * Domain (ASCII, documented for the generator): session_api_calls /
 * max_wait_s / monotonic scripts are numbers or null; _last_activity_desc is
 * a string; route attrs are plain JSON scalars; fixture text is ASCII.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified "tools/delegate_prefix_gate_flow2.c".
 * Every symbol is static dpg2_*.
 *
 * Gate: bash tests/run_one_oracle.sh delegate_prefix_gate_flow2 \
 *           oracle_delegate_prefix_gate_flow2.py "" "lib/libjson/json.o"
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
#define DPG2_FIRST_CHUNK "receiving stream response"
/* delegate_tool_prefix_gate._POLL_S */
#define DPG2_POLL_S 0.05

/* ── universal seam ── */
typedef struct dpg2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} dpg2_io_t;

/* ── truthiness / accessors ── */
static int dpg2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int dpg2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int dpg2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int dpg2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int dpg2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int dpg2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *dpg2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *dpg2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double dpg2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* getattr(d, name, default): a wrapper reads __attr__.name; a present JSON
 * null is a real value (getattr does NOT substitute the default for None). */
static json_t *dpg2_getattr(const json_t *d, const char *name, json_t *defval)
{
    json_t *inner = dpg2_get(d, "__attr__");
    if (inner) {
        json_t *v = json_obj_get(inner, name);
        return v ? v : defval;
    }
    return defval;
}

/* identity: two doubles are the same object iff their __id__ strings match. */
static int dpg2_same_id(const json_t *a, const json_t *b)
{
    const char *ia = dpg2_str(dpg2_get(a, "__id__"));
    const char *ib = dpg2_str(dpg2_get(b, "__id__"));
    if (ia && ib) return strcmp(ia, ib) == 0;
    return a == b;
}

/* JSON deep equality for route-key tuple comparison. */
static int dpg2_json_eq(const json_t *a, const json_t *b)
{
    int na = dpg2_is_null(a), nb = dpg2_is_null(b);
    if (na || nb) return na && nb;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_BOOL:   return (a->bool_val ? 1 : 0) == (b->bool_val ? 1 : 0);
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY: {
            if (json_len(a) != json_len(b)) return 0;
            for (size_t i = 0; i < json_len(a); i++)
                if (!dpg2_json_eq(json_get(a, i), json_get(b, i))) return 0;
            return 1;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !dpg2_json_eq(a->c.items[i], bv)) return 0;
            }
            return 1;
        }
        default: return 0;
    }
}

/* ── raise specs ── */
static int dpg2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *dpg2_kind(const json_t *r)
{
    const char *k = dpg2_str(dpg2_get(r, "__raise__"));
    return k ? k : "";
}

static json_t *dpg2_mkraise(const char *kind, const char *msg)
{
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind ? kind : "RuntimeError"));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}

/* ── FULL builtin exception-parent hierarchy (py3.14, audited against the gate
 *    interpreter's builtins). The BaseException trio (+ CancelledError,
 *    ExceptionGroup/BaseExceptionGroup) parent to BaseException ONLY. ── */
typedef struct { const char *name; const char *parent; } dpg2_exc_row;
static const dpg2_exc_row DPG2_EXC_PARENTS[] = {
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

static const char *dpg2_exc_parent(const char *name)
{
    for (size_t i = 0; DPG2_EXC_PARENTS[i].name; i++)
        if (strcmp(DPG2_EXC_PARENTS[i].name, name) == 0) return DPG2_EXC_PARENTS[i].parent;
    return "Exception";
}

static int dpg2_exc_is_instance(const char *kind, const char *base)
{
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, base) == 0) return 1;
        const char *p = dpg2_exc_parent(cur);
        if (!p || strcmp(cur, p) == 0) break;
        snprintf(cur, sizeof(cur), "%s", p);
        if (strcmp(cur, "BaseException") == 0) return strcmp(base, "BaseException") == 0;
    }
    return 0;
}

static int dpg2_exc_is_exception(const char *kind)
{
    return dpg2_exc_is_instance(kind, "Exception");
}

/* ── printf-style malloc'd format ── */
static char *dpg2_xfmt(const char *fmt, ...)
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

/* ── universal seam call: args owned here (freed after delegating), result
 *    owned by the caller. No seam installed -> fail-closed RuntimeError. ── */
static json_t *dpg2_call(dpg2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = dpg2_xfmt("no seam installed for %s", name);
        r = dpg2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *dpg2_args(void) { return json_object(); }
static void dpg2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* =====================================================================
 * tools/delegate_tool_prefix_gate.py
 * ===================================================================== */

/* _route_key(child): tuple(getattr(child, attr, None) for attr in 4). */
/* helper (the PoP claim sits on dpg2_route_key_fn). */
static json_t *dpg2_route_key_v(const json_t *child)
{
    const char *attrs[4] = {"provider", "base_url", "model", "api_mode"};
    json_t *out = json_array();
    for (int i = 0; i < 4; i++) {
        json_t *v = dpg2_getattr(child, attrs[i], NULL);
        json_append(out, v ? json_copy(v) : json_null());
    }
    return out;
}

/* _caches_prompts(child): getattr(child, "_use_prompt_caching", False) is True. */
/* helper (the PoP claim sits on dpg2_caches_prompts_fn). */
static int dpg2_caches_prompts_v(const json_t *child)
{
    json_t *v = dpg2_getattr(child, "_use_prompt_caching", NULL);
    return dpg2_is_bool(v) && v->bool_val;   /* `is True`, not truthy */
}

/* _first_response_started(child):
 *   (getattr(child,"session_api_calls",0) or 0) >= 1
 *   or getattr(child,"_last_activity_desc",None) == _FIRST_CHUNK_ACTIVITY */
/* helper (the PoP claim sits on dpg2_first_response_started_fn). */
static int dpg2_first_response_started_v(const json_t *child)
{
    json_t *sac = dpg2_getattr(child, "session_api_calls", NULL);
    double v = dpg2_truth(sac) ? dpg2_num(sac) : 0.0;  /* (x or 0) */
    if (v >= 1) return 1;
    json_t *desc = dpg2_getattr(child, "_last_activity_desc", NULL);
    const char *ds = dpg2_str(desc);
    return ds && strcmp(ds, DPG2_FIRST_CHUNK) == 0;
}

/* _stop_requested(child): getattr(child, "_interrupt_requested", False) is True. */
/* helper (the PoP claim sits on dpg2_stop_requested_fn). */
static int dpg2_stop_requested_v(const json_t *child)
{
    json_t *v = dpg2_getattr(child, "_interrupt_requested", NULL);
    return dpg2_is_bool(v) && v->bool_val;
}

/* PrefixCacheGate.__init__(leader, max_wait_s):
 *   self.leader, self.max_wait_s = leader, max_wait_s
 *   self._leader_route = _route_key(leader)
 *   self._leader_done = threading.Event()   (fresh, is_set == False)
 * (helper; the PoP claim sits on dpg2_gate_init_fn). */
static json_t *dpg2_gate_new(const json_t *leader, double mw)
{
    json_t *g = json_object();
    json_set(g, "leader", leader ? json_copy(leader) : json_null());
    json_set(g, "max_wait_s", json_number(mw));
    json_set(g, "leader_route", dpg2_route_key_v(leader));
    json_set(g, "leader_done", json_bool(false));
    return g;
}

/* gate.follows(child): child is not leader AND caches prompts AND
 * _route_key(child) == self._leader_route. */
/* helper (the PoP claim sits on dpg2_gate_follows_fn). */
static int dpg2_gate_follows(const json_t *gate, const json_t *child)
{
    json_t *leader = dpg2_get(gate, "leader");
    if (dpg2_same_id(child, leader)) return 0;
    if (!dpg2_caches_prompts_v(child)) return 0;
    json_t *rk = dpg2_route_key_v(child);
    json_t *lr = dpg2_get(gate, "leader_route");
    int eq = dpg2_json_eq(rk, lr);
    json_free(rk);
    return eq;
}

/* gate._wait(child, parent): poll until leader_done / first response, a stop,
 * or the deadline. time.monotonic + event.wait ride seams; leader_done is a
 * silent state flag. Returns json_bool or a raise spec. */
/* helper (the PoP claim sits on dpg2_gate_wait_fn). */
static json_t *dpg2_gate_wait(dpg2_io_t *io, json_t *gate,
                             const json_t *child, const json_t *parent)
{
    json_t *mono0 = dpg2_call(io, "time.monotonic", dpg2_args());
    if (dpg2_is_raise(mono0)) return mono0;
    double deadline = dpg2_num(mono0) + dpg2_num(dpg2_get(gate, "max_wait_s"));
    json_free(mono0);
    json_t *leader = dpg2_get(gate, "leader");
    for (int iter = 0; iter < 100000; iter++) {
        int done = dpg2_truth(dpg2_get(gate, "leader_done")) || dpg2_first_response_started_v(leader);
        if (done) break;
        if (dpg2_stop_requested_v(child) ||
            (!dpg2_is_null(parent) && dpg2_stop_requested_v(parent)))
            return json_bool(false);
        json_t *mono = dpg2_call(io, "time.monotonic", dpg2_args());
        if (dpg2_is_raise(mono)) return mono;
        double remaining = deadline - dpg2_num(mono);
        json_free(mono);
        if (remaining <= 0) break;
        double t = remaining < DPG2_POLL_S ? remaining : DPG2_POLL_S;
        json_t *a = dpg2_args();
        dpg2_arg_n(a, "timeout", t);
        json_t *wr = dpg2_call(io, "event.wait", a);
        if (dpg2_is_raise(wr)) return wr;
        json_free(wr);
    }
    return json_bool(true);
}

/* ── PoP entry points (universal-seam value calls) ── */

/* PoP: dpg2_route_key_fn @ tools/delegate_tool_prefix_gate.py:_route_key */
static json_t *dpg2_route_key_fn(dpg2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return dpg2_route_key_v(dpg2_get(st, "child"));
}

/* PoP: dpg2_caches_prompts_fn @ tools/delegate_tool_prefix_gate.py:_caches_prompts */
static json_t *dpg2_caches_prompts_fn(dpg2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return json_bool(dpg2_caches_prompts_v(dpg2_get(st, "child")));
}

/* PoP: dpg2_first_response_started_fn @ tools/delegate_tool_prefix_gate.py:_first_response_started */
static json_t *dpg2_first_response_started_fn(dpg2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return json_bool(dpg2_first_response_started_v(dpg2_get(st, "child")));
}

/* PoP: dpg2_stop_requested_fn @ tools/delegate_tool_prefix_gate.py:_stop_requested */
static json_t *dpg2_stop_requested_fn(dpg2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return json_bool(dpg2_stop_requested_v(dpg2_get(st, "child")));
}

/* __init__ entry: construct into st.gate; Python __init__ returns None. */
/* PoP: dpg2_gate_init_fn @ tools/delegate_tool_prefix_gate.py:__init__ */
static json_t *dpg2_gate_init_fn(dpg2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *leader = dpg2_get(in, "leader");
    if (!leader) leader = dpg2_get(st, "leader");
    double mw = dpg2_num(dpg2_get(in, "max_wait_s"));
    json_t *g = dpg2_gate_new(leader, mw);
    json_set(st, "gate", g);
    return json_null();
}

/* for_children: a gate for (index, task, child) triples, or None. */
/* PoP: dpg2_gate_for_children @ tools/delegate_tool_prefix_gate.py:for_children */
static json_t *dpg2_gate_for_children(dpg2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *children = dpg2_get(in, "children");
    double mw = dpg2_num(dpg2_get(in, "max_wait_s"));
    if (mw <= 0) return json_null();
    if (!dpg2_is_arr(children) || json_len(children) < 2) return json_null();
    json_t *tr0 = json_get(children, 0);
    json_t *leader = dpg2_is_arr(tr0) ? json_get(tr0, 2) : NULL;
    if (!dpg2_caches_prompts_v(leader)) return json_null();
    json_t *gate = dpg2_gate_new(leader, mw);
    json_set(st, "gate", json_copy(gate));
    return gate;
}

/* follows entry. */
/* PoP: dpg2_gate_follows_fn @ tools/delegate_tool_prefix_gate.py:follows */
static json_t *dpg2_gate_follows_fn(dpg2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *gate = dpg2_get(st, "gate");
    json_t *child = dpg2_get(in, "child");
    return json_bool(dpg2_gate_follows(gate, child) ? true : false);
}

/* _wait entry. */
/* PoP: dpg2_gate_wait_fn @ tools/delegate_tool_prefix_gate.py:_wait */
static json_t *dpg2_gate_wait_fn(dpg2_io_t *io, const json_t *in, json_t *st)
{
    json_t *gate = dpg2_get(st, "gate");
    return dpg2_gate_wait(io, gate, dpg2_get(in, "child"), dpg2_get(in, "parent"));
}

/* gate.run(child, run, *, parent=None):
 *   leader  -> try run() finally leader_done.set()
 *   other   -> if follows and not _wait -> None; else run() */
/* PoP: dpg2_gate_run @ tools/delegate_tool_prefix_gate.py:run */
static json_t *dpg2_gate_run(dpg2_io_t *io, const json_t *in, json_t *st)
{
    json_t *gate = dpg2_get(st, "gate");
    json_t *child = dpg2_get(in, "child");
    json_t *leader = dpg2_get(gate, "leader");
    json_t *run = dpg2_get(in, "run");
    json_t *parent = dpg2_get(in, "parent");
    const char *run_seam = dpg2_str(dpg2_get(run, "__callable__"));
    if (!run_seam) run_seam = "gate.run_fn";
    if (dpg2_same_id(child, leader)) {
        json_t *r = dpg2_call(io, run_seam, dpg2_args());
        json_set(gate, "leader_done", json_bool(true));  /* finally: set() */
        if (dpg2_is_raise(r)) return r;
        return r ? r : json_null();
    }
    if (dpg2_gate_follows(gate, child)) {
        json_t *w = dpg2_gate_wait(io, gate, child, parent);
        if (dpg2_is_raise(w)) return w;
        int waited = dpg2_truth(w);
        json_free(w);
        if (!waited) return json_null();
    }
    json_t *r = dpg2_call(io, run_seam, dpg2_args());
    if (dpg2_is_raise(r)) return r;
    return r ? r : json_null();
}
