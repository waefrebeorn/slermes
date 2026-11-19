/* relay_metrics_flow.c — FLOW + leaf port of the UNCLAIMED third-wave remainder
 * of hermes_cli/observability/relay_shared_metrics.py (the relay-metrics module;
 * the task brief named "relay_metrics.py", which does not exist in the quarry —
 * per the skill's defining-module rule every PoP below names the REAL defining
 * module). The claimed siblings live in relay_shared_metrics.c / _flow.c /
 * src/cli/relay_shared_metrics_flow2.c / _gaps and are SEAMED here through the
 * injected universal seam rm_io_t.
 *
 * Decision-sequence port (25 functions): the install-snapshot / mark / scoped-run
 * emission core, the flush-worker lifecycle (schedule / worker / flush-and-export
 * / owner-scope context manager), the close-and-export teardown, the lineage
 * sharing / route-run / segment-close / summary-row cluster, the send-pass
 * export path, the auxiliary-token / switch-after / friction recorders, the
 * store-consent reconciliation (nested context managers) and its once-per-process
 * gate, and the four module-level runtime-gate wrappers.
 * ALL collaborator I/O (claimed siblings, foreign-module imports, the Relay
 * host/subscriber/store/thread/clock, logger, the _RUNTIMES registry gates,
 * hermes_constants home overrides, pathlib exists) rides the universal seam;
 * the harness scripts results and records the ordered trace the oracle diffs
 * against LIVE relay_shared_metrics.py under the same scripts. Ported functions
 * call each other REAL (internal composition: _emit_install_snapshot_if_due ->
 * _mark -> _run_scoped; _run_flush_worker -> _flush_and_export -> _owner_scope;
 * _close_and_export -> _flush_and_export; _close_unseen_segment -> _close_segment
 * + _emit_summary_rows; _flush_lineage -> _emit_summary_rows; record_switch_after
 * -> _with_route_run; _reconcile_send_consent_once -> _reconcile_store_consent).
 *
 * Representation contract (shared with tests/oracle_relay_metrics_flow.py):
 *   - raise specs are VALUES {"__raise__": kind, "message": msg} threaded through
 *     every helper; an uncaught one becomes the case ret. The BaseException
 *     family (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError /
 *     BaseExceptionGroup) passes through every `except Exception` guard exactly
 *     as live; rm_exc_is_exception walks the FULL builtin exception-parent table.
 *   - fail-closed: an absent / exhausted seam script returns a RuntimeError raise
 *     spec ("no script for <name>"), recorded as the event ret BEFORE propagation.
 *   - object doubles (session/task/lineage/thread/store/resolved/efficiency) are
 *     attribute dicts read via rm_get; tuple/set -> array (sets sorted); None ->
 *     null; integral floats collapse to ints on both sides.
 *   - callable doubles {"__callable__": "<seam>"} fire that seam with
 *     {"args": [...], "kwargs": {...}} when invoked. The still_consented closure
 *     of _run_send_pass projects to {"__callable__": "still_consented"} (the
 *     scripted sender double never invokes it; its body is out of seam scope).
 *   - context-manager doubles {"__cm__": "<name>"} fire "<name>.__enter__" (args
 *     {}) and "<name>.__exit__" (args {exc: kind-or-null}); a truthy exit result
 *     suppresses a pending body raise, an exit raise OVERRIDES it. The ported
 *     _owner_scope is transcribed (set/reset_hermes_home_override seams around
 *     the body; a reset raise overrides a body raise; nothing is suppressed).
 *   - scripts: a bare JSON array is a per-call QUEUE (one result per call); a
 *     list RESULT must be wrapped {"__script__": true, "ret": [...]}. A scripted
 *     null returns None. None-valued seam-arg OBJECT keys dropped on both sides.
 *   - fixture domain (ASCII scope): object seam results are null or a record;
 *     attribute doubles carry the fields the live body reads (state models are
 *     documented per function in the oracle); home paths are normalized POSIX
 *     strings (no "."/".."/double-slash components — PurePosixPath join is
 *     modelled as separator-aware concat); run.take results are lists or null
 *     (the *taken splat domain); tools.rows results are lists of [mark, data]
 *     pairs; CM factory results are {"__cm__": name} markers or null (null ->
 *     the live TypeError). EXCLUDED: integral-FLOAT literals in str() positions
 *     (libjson has no int/float identity); non-marker CM factory dicts; set
 *     iteration order is unobservable here (only membership / sorted echoes).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/relay_metrics_flow.c". rsm_ / rsm_flow_ / rsm2_ prefixes are
 * taken by the sibling ports; this port uses rm_.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

/* ── universal seam ── */
typedef struct rm_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} rm_io_t;

/* ── truthiness / accessors (Python-faithful; no libjson string coercion) ── */
static bool rm_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool rm_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rm_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool rm_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool rm_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool rm_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool rm_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}

static const char *rm_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *rm_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double rm_num(const json_t *o, const char *k, double def)
{
    const json_t *v = rm_get(o, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static bool rm_bool(const json_t *o, const char *k, bool def)
{
    const json_t *v = rm_get(o, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : def;
}

static bool rm_str_eq(const char *a, const char *b)
{
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static json_t *rm_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* ── raise specs ── */
static bool rm_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *rm_kind(const json_t *r)
{
    const char *k = rm_str(rm_get(r, "__raise__"));
    return k ? k : "";
}
static json_t *rm_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *rm_xfmt(const char *fmt, ...)
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

/* ── FULL builtin exception-parent table (py3.14). A kind is an Exception
 * iff walking its parents reaches Exception before BaseException. Unknown
 * kinds are synthetic Exception subclasses in the oracle (make_exc), so they
 * default to the Exception family. ── */
typedef struct rm_exc_parent { const char *kind; const char *parent; } rm_exc_parent_t;
static const rm_exc_parent_t RM_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
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
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"WindowsError", "OSError"},
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
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"ResourceWarning", "Warning"},
};
static bool rm_exc_is_exception(const char *kind)
{
    char cur[128];
    snprintf(cur, sizeof cur, "%s", kind ? kind : "");
    for (int hops = 0; hops < 16; hops++) {
        if (rm_str_eq(cur, "Exception")) return true;
        if (rm_str_eq(cur, "BaseException") || rm_str_eq(cur, "")) return false;
        const char *parent = NULL;
        for (size_t i = 0; i < sizeof(RM_EXC_PARENTS) / sizeof(RM_EXC_PARENTS[0]); i++) {
            if (rm_str_eq(RM_EXC_PARENTS[i].kind, cur)) { parent = RM_EXC_PARENTS[i].parent; break; }
        }
        if (!parent) return true;   /* unknown synthetic kind: Exception subclass */
        snprintf(cur, sizeof cur, "%s", parent);
    }
    return false;
}
static bool rm_is_base(const char *kind) { return !rm_exc_is_exception(kind); }

/* ── universal seam call: args OWNED by caller, freed here; result caller-owned
 * (NULL == None, or a raise spec). Fail-closed when no channel installed. ── */
static json_t *rm_call(rm_io_t *io, const char *name, json_t *args)
{
    if (!io || !io->call) {
        char *m = rm_xfmt("no seam installed for %s", name);
        json_t *r = rm_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* arg builders: None-valued keys dropped on both sides ("" kept). */
static void rm_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void rm_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void rm_arg_b(json_t *a, const char *k, bool v)
{
    json_set(a, k, json_bool(v));
}
static void rm_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* logger seam: {msg, args:[], exc_info}. */
static json_t *rm_log(rm_io_t *io, const char *level, const char *msg, bool exc_info)
{
    json_t *a = json_object();
    rm_arg_s(a, "msg", msg);
    json_set(a, "args", json_array());
    if (exc_info) rm_arg_b(a, "exc_info", true);
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    return rm_call(io, name, a);
}

/* invoke a callable double {"__callable__": seam} with positional/keyword args.
 * args_arr / kwargs_obj ownership transferred (consumed). */
static json_t *rm_invoke_callable(rm_io_t *io, const json_t *cb,
                                  json_t *args_arr, json_t *kwargs_obj)
{
    const char *name = rm_str(rm_get(cb, "__callable__"));
    if (!name) {
        json_free(args_arr);
        json_free(kwargs_obj);
        return rm_mkraise("TypeError", "'NoneType' object is not callable");
    }
    json_t *a = json_object();
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
    return rm_call(io, name, a);
}

/* attribute read on a double: present value (copy) or the live AttributeError.
 * null receiver -> NoneType message; absent field -> SimpleNamespace message. */
static json_t *rm_read_attr(const json_t *obj, const char *key)
{
    if (rm_is_null(obj)) {
        char *m = rm_xfmt("'NoneType' object has no attribute '%s'", key);
        json_t *r = rm_mkraise("AttributeError", m ? m : "");
        free(m);
        return r;
    }
    const json_t *v = rm_get(obj, key);
    if (!v) {
        char *m = rm_xfmt("'types.SimpleNamespace' object has no attribute '%s'", key);
        json_t *r = rm_mkraise("AttributeError", m ? m : "");
        free(m);
        return r;
    }
    return json_copy(v);
}

/* PurePosixPath join for the normalized-base domain (no "."/".."/"//" parts):
 * separator-aware concat; empty base yields the relative suffix. */
static char *rm_posix_join(const char *base, const char *suffix)
{
    if (!base) base = "";
    size_t bl = strlen(base);
    bool need_sep = bl > 0 && base[bl - 1] != '/';
    return rm_xfmt("%s%s%s", base, need_sep ? "/" : "", suffix ? suffix : "");
}

/* ── context-manager doubles {"__cm__": name}: enter/exit seams ── */
static json_t *rm_cm_enter(rm_io_t *io, const json_t *cm)
{
    const char *name = rm_str(rm_get(cm, "__cm__"));
    if (!name)
        return rm_mkraise("TypeError",
            "'NoneType' object does not support the context manager protocol (missed __exit__ method)");
    char *nm = rm_xfmt("%s.__enter__", name);
    json_t *r = rm_call(io, nm ? nm : "__enter__", json_object());
    free(nm);
    return r;
}
static json_t *rm_cm_exit(rm_io_t *io, const json_t *cm, const char *exc_kind)
{
    const char *name = rm_str(rm_get(cm, "__cm__"));
    if (!name)
        return rm_mkraise("TypeError",
            "'NoneType' object does not support the context manager protocol (missed __exit__ method)");
    char *nm = rm_xfmt("%s.__exit__", name);
    json_t *a = json_object();
    rm_arg_s(a, "exc", exc_kind);
    json_t *r = rm_call(io, nm ? nm : "__exit__", a);
    free(nm);
    return r;
}
/* a null CM factory result: live `with None:` TypeError */
static json_t *rm_cm_check(const json_t *factory_result)
{
    if (rm_is_null(factory_result))
        return rm_mkraise("TypeError",
            "'NoneType' object does not support the context manager protocol (missed __exit__ method)");
    return NULL;
}

/* ── module-level runtime-gate helpers ── */
/* isinstance(runtime, _Runtime): a registry value marked {"__runtime__": true}. */
static bool rm_is_runtime(const json_t *v)
{
    return rm_is_obj(v) && rm_bool(v, "__runtime__", false);
}
/* runtime._safe(callback, *args, **kwargs) — seam; callback projected as a
 * callable double, runtime_id carried for trace disambiguation. */
static json_t *rm_rt_safe(rm_io_t *io, const json_t *runtime, const char *cb_name,
                          json_t *args_arr, json_t *kwargs_obj)
{
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string(cb_name));
    json_t *a = json_object();
    rm_arg_j(a, "callback", cb);
    json_free(cb);
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
    const char *rid = rm_str(rm_get(runtime, "id"));
    rm_arg_s(a, "runtime_id", rid);
    return rm_call(io, "rt._safe", a);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 1. _run_scoped — task context when given, else the session's host scope
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rm_run_scoped_core(rm_io_t *io, const json_t *session, const json_t *task,
                                  const json_t *callback, json_t *args_arr, json_t *kwargs_obj)
{
    json_t *a = json_object();
    if (!rm_is_null(task)) {
        rm_arg_j(a, "task", task);
        rm_arg_j(a, "callback", callback);
        json_set(a, "args", args_arr ? args_arr : json_array());
        json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
        return rm_call(io, "rt._run_in_task", a);
    }
    /* host.run_in_session(session.relay_session, callback, *args, **kwargs) —
     * a direct attribute read: absent relay_session raises BEFORE the seam. */
    if (rm_is_null(session)) {
        json_free(args_arr);
        json_free(kwargs_obj);
        return rm_mkraise("AttributeError", "'NoneType' object has no attribute 'relay_session'");
    }
    const json_t *relay_session = rm_get(session, "relay_session");
    if (!relay_session) {
        json_free(args_arr);
        json_free(kwargs_obj);
        return rm_mkraise("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'relay_session'");
    }
    rm_arg_j(a, "relay_session", relay_session);
    rm_arg_j(a, "callback", callback);
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
    return rm_call(io, "host.run_in_session", a);
}
/* PoP: rm_run_scoped @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._run_scoped */
static json_t *rm_run_scoped(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *session = rm_get(in, "session");
    const json_t *task = rm_get(in, "task");
    const json_t *callback = rm_get(in, "callback");
    json_t *args_arr = json_array();
    const json_t *ia = rm_get(in, "args");
    if (rm_is_arr(ia))
        for (size_t i = 0; i < ia->c.count; i++)
            json_append(args_arr, json_copy(ia->c.items[i]));
    json_t *kwargs_obj = json_object();
    const json_t *ik = rm_get(in, "kwargs");
    if (rm_is_obj(ik))
        for (size_t i = 0; i < ik->c.count; i++)
            json_set(kwargs_obj, ik->c.keys[i], json_copy(ik->c.items[i]));
    return rm_run_scoped_core(io, session, task, callback, args_arr, kwargs_obj);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. _mark — one Relay mark under the task/session scope. Kwarg evaluation
 * order: handle=_scope_handle(...) seam, then metadata=_event_metadata() seam,
 * then the _run_scoped composition.
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rm_mark_core(rm_io_t *io, const json_t *session, const json_t *task,
                            const char *name, const json_t *data)
{
    json_t *ha = json_object();
    rm_arg_j(ha, "session", session);
    rm_arg_j(ha, "task", task);
    json_t *handle = rm_call(io, "_scope_handle", ha);
    if (rm_is_raise(handle)) return handle;
    json_t *meta = rm_call(io, "rt._event_metadata", json_object());
    if (rm_is_raise(meta)) { json_free(handle); return meta; }
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("relay.scope.event"));
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(name ? name : ""));
    json_t *kwargs_obj = json_object();
    rm_arg_j(kwargs_obj, "handle", handle);
    json_free(handle);
    rm_arg_j(kwargs_obj, "data", data);
    rm_arg_j(kwargs_obj, "metadata", meta);
    json_free(meta);
    json_t *r = rm_run_scoped_core(io, session, task, cb, args_arr, kwargs_obj);
    json_free(cb);
    return r;
}
/* PoP: rm_mark @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._mark */
static json_t *rm_mark(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *r = rm_mark_core(io, rm_get(in, "session"), rm_get(in, "task"),
                             json_get_str(in, "name", ""), rm_get(in, "data"));
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. _emit_install_snapshot_if_due — hourly store latch; snapshot mark
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_emit_install_snapshot_if_due @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._emit_install_snapshot_if_due */
static json_t *rm_emit_install_snapshot_if_due(rm_io_t *io, const json_t *in, json_t *st)
{
    const json_t *session = rm_get(in, "session");
    json_t *now = rm_call(io, "monotonic_ns", json_object());
    if (rm_is_raise(now)) return now;
    double now_ns = rm_is_num(now) ? now->num_val : 0.0;
    json_free(now);
    const json_t *checked = rm_get(st, "_snapshot_checked_ns");
    if (!rm_is_null(checked) && rm_is_num(checked)
        && now_ns - checked->num_val < 3600.0 * 1000000000.0)
        return NULL;
    json_set(st, "_snapshot_checked_ns", json_number(now_ns));
    json_t *due = rm_call(io, "store.install_snapshot_due", json_object());
    if (rm_is_raise(due)) return due;
    bool is_due = rm_truth(due);
    json_free(due);
    if (!is_due) return NULL;
    /* from .shared_metrics_snapshot import collect_install_snapshot */
    json_t *cfg = rm_call(io, "_raw_config", json_object());
    if (rm_is_raise(cfg)) return cfg;
    json_t *ca = json_object();
    rm_arg_j(ca, "config", cfg);
    json_free(cfg);
    json_t *fields = rm_call(io, "collect_install_snapshot", ca);
    if (rm_is_raise(fields)) return fields;
    json_t *r = rm_mark_core(io, session, NULL, "hermes.install.snapshot", fields);
    json_free(fields);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. _owner_scope — profile-home override context manager (transcribed CM:
 * set seam, body, reset seam in finally; reset raise overrides body raise)
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rm_owner_scope_core(rm_io_t *io, json_t *st,
                                   const json_t *body_cb, bool body_is_export)
{
    const json_t *host = rm_get(st, "host");
    const json_t *pk = rm_get(host, "profile_key");
    if (!host || !pk) {
        json_t *r = rm_mkraise("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'profile_key'");
        return r;
    }
    json_t *a = json_object();
    rm_arg_j(a, "profile_key", pk);
    json_t *token = rm_call(io, "set_hermes_home_override", a);
    if (rm_is_raise(token)) return token;
    json_t *body_raise = NULL;
    if (body_is_export) {
        json_t *r = rm_call(io, "rt._export", json_object());
        if (rm_is_raise(r)) body_raise = r; else json_free(r);
    } else if (!rm_is_null(body_cb)) {
        json_t *r = rm_invoke_callable(io, body_cb, json_array(), json_object());
        if (rm_is_raise(r)) body_raise = r; else json_free(r);
    }
    json_t *ra = json_object();
    rm_arg_j(ra, "token", token);
    json_free(token);
    json_t *rr = rm_call(io, "reset_hermes_home_override", ra);
    if (rm_is_raise(rr)) { json_free(body_raise); return rr; }
    json_free(rr);
    if (body_raise) return body_raise;
    return NULL;
}
/* PoP: rm_owner_scope @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._owner_scope */
static json_t *rm_owner_scope(rm_io_t *io, const json_t *in, json_t *st)
{
    return rm_owner_scope_core(io, st, rm_get(in, "body"), false);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. _flush_and_export — flush; on Exception log+skip, else export under the
 * owner scope (BaseException passes through unlogged)
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rm_flush_and_export_core(rm_io_t *io, const char *failure_message, json_t *st)
{
    json_t *fl = rm_call(io, "relay.subscribers.flush", json_object());
    if (rm_is_raise(fl)) {
        if (rm_is_base(rm_kind(fl))) return fl;
        json_free(fl);
        json_t *lr = rm_log(io, "warning", failure_message, true);
        if (rm_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    json_free(fl);
    return rm_owner_scope_core(io, st, NULL, true);
}
/* PoP: rm_flush_and_export @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._flush_and_export */
static json_t *rm_flush_and_export(rm_io_t *io, const json_t *in, json_t *st)
{
    return rm_flush_and_export_core(io, json_get_str(in, "failure_message", ""), st);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. _run_flush_worker — coalescing loop: pending -> one flush pass -> exit
 * (fixture domain: at most one pass; the flush seams never re-set pending)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_run_flush_worker @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._run_flush_worker */
static json_t *rm_run_flush_worker(rm_io_t *io, const json_t *in, json_t *st)
{
    for (;;) {
        bool pending = rm_truth(rm_get(st, "_flush_pending"));
        if (!pending) {
            json_set(st, "_flush_thread", json_null());
            return NULL;
        }
        json_set(st, "_flush_pending", json_bool(false));
        json_t *r = rm_flush_and_export_core(io, json_get_str(in, "failure_message", ""), st);
        if (rm_is_raise(r)) return r;
        json_free(r);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * 7. _schedule_flush_and_export — pending flag; one worker thread per runtime
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_schedule_flush_and_export @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._schedule_flush_and_export */
static json_t *rm_schedule_flush_and_export(rm_io_t *io, const json_t *in, json_t *st)
{
    /* from agent.memory_provider import spawn_context_thread (stub resolves) */
    json_set(st, "_flush_pending", json_bool(true));
    const json_t *existing = rm_get(st, "_flush_thread");
    if (!rm_is_null(existing)) return NULL;
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("rt._run_flush_worker"));
    json_t *a = json_object();
    rm_arg_j(a, "callback", cb);
    json_free(cb);
    rm_arg_s(a, "name", "hermes-shared-metrics-flush");
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(json_get_str(in, "failure_message", "")));
    json_set(a, "args", args_arr);
    json_t *thread = rm_call(io, "spawn_context_thread", a);
    if (rm_is_raise(thread)) return thread;
    json_t *sr = rm_call(io, "thread.start", json_object());
    if (rm_is_raise(sr)) { json_free(thread); return sr; }
    json_free(sr);
    json_set(st, "_flush_thread", thread);   /* ownership transfer */
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 8. _close_and_export — deactivate; close sessions + unseen segments; flush,
 * deregister, release (only when registered)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_close_and_export @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._close_and_export */
static json_t *rm_close_and_export(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_set(st, "_active", json_bool(false));
    const json_t *sessions = rm_get(st, "_sessions");
    size_t n = rm_is_obj(sessions) ? sessions->c.count : 0;
    char **sids = NULL;
    if (n) {
        sids = (char **)malloc(n * sizeof(char *));
        if (sids) for (size_t i = 0; i < n; i++) sids[i] = strdup(sessions->c.keys[i]);
    }
    for (size_t i = 0; i < n; i++) {
        json_t *event = json_object();
        json_set(event, "session_id", json_string(sids[i]));
        json_t *args_arr = json_array();
        json_append(args_arr, event);
        json_t *r = rm_rt_safe(io, st, "rt.close_session", args_arr, json_object());
        if (rm_is_raise(r)) {
            for (size_t j = i; j < n; j++) free(sids[j]);
            free(sids);
            return r;
        }
        json_free(r);
    }
    for (size_t j = 0; j < n; j++) free(sids[j]);
    free(sids);
    const json_t *lineage_of = rm_get(st, "_lineage_of");
    size_t m = rm_is_obj(lineage_of) ? lineage_of->c.count : 0;
    char **unseen = NULL;
    if (m) {
        unseen = (char **)malloc(m * sizeof(char *));
        if (unseen) for (size_t i = 0; i < m; i++) unseen[i] = strdup(lineage_of->c.keys[i]);
    }
    for (size_t i = 0; i < m; i++) {
        json_t *args_arr = json_array();
        json_append(args_arr, json_string(unseen[i]));
        json_t *r = rm_rt_safe(io, st, "rt._close_unseen_segment", args_arr, json_object());
        if (rm_is_raise(r)) {
            for (size_t j = i; j < m; j++) free(unseen[j]);
            free(unseen);
            return r;
        }
        json_free(r);
    }
    for (size_t j = 0; j < m; j++) free(unseen[j]);
    free(unseen);
    if (!rm_truth(rm_get(st, "_registered"))) return NULL;
    json_t *r = rm_flush_and_export_core(io, "Hermes shared-metrics shutdown flush failed", st);
    if (rm_is_raise(r)) return r;
    json_free(r);
    json_t *dr = rm_call(io, "rt._deregister", json_object());
    if (rm_is_raise(dr)) return dr;
    json_free(dr);
    json_t *rr = rm_call(io, "rt._release", json_object());
    if (rm_is_raise(rr)) return rr;
    json_free(rr);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 9. _share_spent — a rotated-to segment inherits the lineage's spent turns
 * and connector chain
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_share_spent @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._share_spent */
static json_t *rm_share_spent(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *session = rm_get(st, "session");
    const char *sid = rm_str(rm_get(session, "session_id"));
    json_t *a = json_object();
    rm_arg_s(a, "session_id", sid);
    json_t *spent = rm_call(io, "rt._lineage_spent", a);
    if (rm_is_raise(spent)) return spent;
    if (!rm_is_null(spent)) {
        json_t *efficiency = rm_get(session, "efficiency");
        if (rm_is_obj(efficiency)) json_set(efficiency, "spent", spent);
        else json_free(spent);
    } else {
        json_free(spent);
    }
    const json_t *lineage_of = rm_get(st, "_lineage_of");
    const char *key = rm_str(rm_get(lineage_of, sid ? sid : ""));
    const json_t *lineages = rm_get(st, "_lineages");
    const json_t *lineage = rm_get(lineages, key ? key : "");
    if (!rm_is_null(lineage)) {
        const json_t *connectors = rm_get(lineage, "connectors");
        if (!rm_is_null(connectors))
            json_set(session, "connectors", json_copy(connectors));
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 10. _with_route_run — apply update to the lineage's run (else own)
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rm_with_route_run_core(rm_io_t *io, json_t *st, const char *session_id,
                                      const json_t *own, const json_t *update_cb, bool take_mode)
{
    const json_t *lineage_of = rm_get(st, "_lineage_of");
    const char *key = rm_str(rm_get(lineage_of, session_id ? session_id : ""));
    const json_t *lineages = rm_get(st, "_lineages");
    const json_t *lineage = rm_get(lineages, key ? key : "");
    const json_t *run = !rm_is_null(lineage) ? rm_get(lineage, "run") : own;
    if (rm_is_null(run)) return NULL;
    if (take_mode) {
        /* lambda run: run.take() — the run double's take recorder, no args */
        return rm_call(io, "run.take", json_object());
    }
    json_t *args_arr = json_array();
    json_append(args_arr, json_copy(run));
    return rm_invoke_callable(io, update_cb, args_arr, json_object());
}
/* PoP: rm_with_route_run @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._with_route_run */
static json_t *rm_with_route_run(rm_io_t *io, const json_t *in, json_t *st)
{
    return rm_with_route_run_core(io, st, json_get_str(in, "session_id", ""),
                                  rm_get(in, "own"), rm_get(in, "update"), false);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 11. _close_segment — detach a segment; merge peak/tally/tools into the
 * lineage; the merged triple only once the last open segment closes
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rm_close_segment_core(rm_io_t *io, const json_t *in, json_t *st)
{
    const char *session_id = json_get_str(in, "session_id", "");
    const json_t *state = rm_get(in, "segment_state");
    const json_t *tally = rm_get(in, "tally");
    const json_t *tools = rm_get(in, "tools");
    json_t *lineage_of = rm_get(st, "_lineage_of");
    const json_t *keynode = rm_get(lineage_of, session_id);
    const char *key = rm_str(keynode);
    if (keynode) json_obj_del(lineage_of, session_id);   /* pop */
    json_t *lineages = rm_get(st, "_lineages");
    const json_t *lineage = (key && !rm_is_null(keynode)) ? rm_get(lineages, key) : NULL;
    if (rm_is_null(lineage)) {
        json_t *out = json_array();
        json_append(out, rm_copy_or_null(state));
        json_append(out, rm_copy_or_null(tally));
        json_append(out, rm_copy_or_null(tools));
        return out;
    }
    /* lineage.open.discard(session_id) — sorted-array set model */
    json_t *open = rm_get(lineage, "open");
    if (rm_is_arr(open)) {
        for (size_t i = 0; i < open->c.count; i++) {
            if (rm_str_eq(rm_str(open->c.items[i]), session_id)) {
                json_array_remove(open, i);
                break;
            }
        }
    }
    if (!rm_is_null(state) && !rm_is_null(tally)) {
        json_t *aa = json_object();
        rm_arg_j(aa, "into", rm_get(lineage, "peak"));
        rm_arg_j(aa, "segment", state);
        json_t *r = rm_call(io, "_absorb_peak", aa);
        if (rm_is_raise(r)) return r;
        json_free(r);
        json_t *ta = json_object();
        rm_arg_j(ta, "tally", tally);
        r = rm_call(io, "tally.absorb", ta);
        if (rm_is_raise(r)) return r;
        json_free(r);
    }
    json_t *toa = json_object();
    rm_arg_j(toa, "tools", tools);
    json_t *r = rm_call(io, "tools.absorb", toa);
    if (rm_is_raise(r)) return r;
    json_free(r);
    if (rm_truth(open)) {
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    json_obj_del(lineages, key);
    json_t *out = json_array();
    json_append(out, rm_copy_or_null(rm_get(lineage, "peak")));
    json_append(out, rm_copy_or_null(rm_get(lineage, "tally")));
    json_append(out, rm_copy_or_null(rm_get(lineage, "tools")));
    return out;
}
/* PoP: rm_close_segment @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._close_segment */
static json_t *rm_close_segment(rm_io_t *io, const json_t *in, json_t *st)
{
    return rm_close_segment_core(io, in, st);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 12. _emit_summary_rows — peak/tally/tool rows + extras, null-data filtered,
 * one guarded _emit_rows call (claimed sibling seam)
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rm_emit_summary_rows_core(rm_io_t *io, const json_t *session,
                                         const json_t *peak, const json_t *tally,
                                         const json_t *tools, const json_t *extra)
{
    json_t *marks = json_array();
    json_t *row = json_array();
    json_append(row, json_string("hermes.context_peak"));
    if (!rm_is_null(peak)) {
        json_t *f = rm_call(io, "peak.context_peak_fields", json_object());
        if (rm_is_raise(f)) { json_free(row); json_free(marks); return f; }
        json_append(row, f ? f : json_null());
    } else {
        json_append(row, json_null());
    }
    json_append(marks, row);
    row = json_array();
    json_append(row, json_string("hermes.session"));
    if (!rm_is_null(tally)) {
        json_t *f = rm_call(io, "tally.fields", json_object());
        if (rm_is_raise(f)) { json_free(row); json_free(marks); return f; }
        json_append(row, f ? f : json_null());
    } else {
        json_append(row, json_null());
    }
    json_append(marks, row);
    if (!rm_is_null(tools)) {
        json_t *rows = rm_call(io, "tools.rows", json_object());
        if (rm_is_raise(rows)) { json_free(marks); return rows; }
        if (rm_is_arr(rows))
            for (size_t i = 0; i < rows->c.count; i++)
                json_append(marks, json_copy(rows->c.items[i]));
        json_free(rows);
    }
    if (rm_is_arr(extra))
        for (size_t i = 0; i < extra->c.count; i++)
            json_append(marks, json_copy(extra->c.items[i]));
    /* [(mark, data) for mark, data in marks if data is not None] */
    json_t *filtered = json_array();
    for (size_t i = 0; i < marks->c.count; i++) {
        const json_t *mr = marks->c.items[i];
        const json_t *data = rm_is_arr(mr) ? json_get(mr, 1) : NULL;
        if (data && data->type != JSON_NULL)
            json_append(filtered, json_copy(mr));
    }
    json_free(marks);
    json_t *a = json_object();
    rm_arg_j(a, "session", session);
    json_set(a, "rows", filtered);
    json_t *r = rm_call(io, "rt._emit_rows", a);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}
/* PoP: rm_emit_summary_rows @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._emit_summary_rows */
static json_t *rm_emit_summary_rows(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return rm_emit_summary_rows_core(io, rm_get(in, "session"), rm_get(in, "peak"),
                                     rm_get(in, "tally"), rm_get(in, "tools"),
                                     rm_get(in, "extra"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 13. _close_unseen_segment — close a never-served rotated segment
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_close_unseen_segment @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._close_unseen_segment */
static json_t *rm_close_unseen_segment(rm_io_t *io, const json_t *in, json_t *st)
{
    const char *session_id = json_get_str(in, "session_id", "");
    if (!session_id[0]) return NULL;
    json_t *tools = rm_call(io, "eff.ToolUsage", json_object());
    if (rm_is_raise(tools)) return tools;
    json_t *ci = json_object();
    rm_arg_s(ci, "session_id", session_id);
    json_set(ci, "segment_state", json_null());
    json_set(ci, "tally", json_null());
    json_set(ci, "tools", tools ? tools : json_null());
    json_t *triple = rm_close_segment_core(io, ci, st);
    json_free(ci);
    if (rm_is_raise(triple)) return triple;
    json_t *si = json_object();
    json_set(si, "session", json_null());
    json_set(si, "peak", json_copy(json_get(triple, 0)));
    json_set(si, "tally", json_copy(json_get(triple, 1)));
    json_set(si, "tools", json_copy(json_get(triple, 2)));
    json_free(triple);
    json_t *r = rm_emit_summary_rows_core(io, NULL, rm_get(si, "peak"),
                                          rm_get(si, "tally"), rm_get(si, "tools"), NULL);
    json_free(si);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 14. _flush_lineage — backstop: report a conversation, detach its segments
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_flush_lineage @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._flush_lineage */
static json_t *rm_flush_lineage(rm_io_t *io, const json_t *in, json_t *st)
{
    const char *key = json_get_str(in, "key", "");
    json_t *lineages = rm_get(st, "_lineages");
    const json_t *lineage = rm_get(lineages, key);
    int present = lineage != NULL;
    int had = !rm_is_null(lineage);   /* pop returns a present-null as None */
    json_t *lineage_copy = had ? json_copy(lineage) : NULL;
    if (present) json_obj_del(lineages, key);   /* pop removes even a null value */
    if (had) {
        const json_t *open = rm_get(lineage_copy, "open");
        json_t *lineage_of = rm_get(st, "_lineage_of");
        if (rm_is_arr(open))
            for (size_t i = 0; i < open->c.count; i++) {
                const char *seg = rm_str(open->c.items[i]);
                if (seg && rm_get(lineage_of, seg)) json_obj_del(lineage_of, seg);
            }
    }
    json_t *r = NULL;
    if (had) {
        r = rm_emit_summary_rows_core(io, NULL, rm_get(lineage_copy, "peak"),
                                      rm_get(lineage_copy, "tally"),
                                      rm_get(lineage_copy, "tools"), NULL);
    }
    json_free(lineage_copy);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 15. _send_exported_packages — consent edge, send gate, one in-flight pass
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_send_exported_packages @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._send_exported_packages */
static json_t *rm_send_exported_packages(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *resolved = rm_call(io, "_resolved_send_config", json_object());
    if (rm_is_raise(resolved)) {
        if (rm_is_base(rm_kind(resolved))) return resolved;
        json_free(resolved);
        json_t *lr = rm_log(io, "debug", "Unable to read shared-metrics send policy", true);
        if (rm_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    /* resolved.send is evaluated as a _guarded ARGUMENT before the seam fires */
    json_t *send = rm_read_attr(resolved, "send");
    if (rm_is_raise(send)) { json_free(resolved); return send; }
    const json_t *subscriber = rm_get(st, "subscriber");
    const json_t *store = rm_get(subscriber, "store");
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("_reconcile_store_consent"));
    json_t *ga = json_object();
    rm_arg_s(ga, "message", "Unable to record a shared-metrics consent transition");
    rm_arg_j(ga, "callback", cb);
    json_free(cb);
    json_t *gargs = json_array();
    json_append(gargs, rm_copy_or_null(store));
    json_append(gargs, json_copy(send));
    json_set(ga, "args", gargs);
    json_set(ga, "kwargs", json_object());
    json_t *gr = rm_call(io, "rt._guarded", ga);
    if (rm_is_raise(gr)) { json_free(send); json_free(resolved); return gr; }
    json_free(gr);
    bool send_enabled = rm_truth(send);
    json_free(send);
    if (!send_enabled) { json_free(resolved); return NULL; }
    const json_t *thread = rm_get(st, "_send_thread");
    bool spawn = rm_is_null(thread);
    if (!spawn) {
        json_t *alive = rm_call(io, "thread.is_alive", json_object());
        if (rm_is_raise(alive)) { json_free(resolved); return alive; }
        spawn = !rm_truth(alive);
        json_free(alive);
    }
    if (spawn) {
        json_t *endpoint = rm_read_attr(resolved, "endpoint");
        if (rm_is_raise(endpoint)) { json_free(resolved); return endpoint; }
        json_t *scb = json_object();
        json_set(scb, "__callable__", json_string("rt._run_send_pass"));
        json_t *a = json_object();
        rm_arg_j(a, "callback", scb);
        json_free(scb);
        rm_arg_s(a, "name", "hermes-shared-metrics-send");
        json_t *args_arr = json_array();
        json_append(args_arr, endpoint);   /* ownership transfer */
        json_set(a, "args", args_arr);
        json_t *nt = rm_call(io, "spawn_context_thread", a);
        if (rm_is_raise(nt)) { json_free(resolved); return nt; }
        /* live assigns self._send_thread BEFORE calling .start() on it */
        json_set(st, "_send_thread", nt);
        json_t *sr = rm_call(io, "thread.start", json_object());
        if (rm_is_raise(sr)) { json_free(resolved); return sr; }
        json_free(sr);
    }
    json_free(resolved);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 16. _run_send_pass — construct the sender (consent closure projected), then
 * one guarded send_pending
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_run_send_pass @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._run_send_pass */
static json_t *rm_run_send_pass(rm_io_t *io, const json_t *in, json_t *st)
{
    /* from hermes_cli.observability.shared_metrics_sender import SharedMetricsSender */
    const json_t *subscriber = rm_get(st, "subscriber");
    const json_t *store = rm_get(subscriber, "store");
    json_t *consent = json_object();
    json_set(consent, "__callable__", json_string("still_consented"));
    json_t *a = json_object();
    rm_arg_j(a, "store", store);
    rm_arg_s(a, "endpoint", json_get_str(in, "endpoint", ""));
    rm_arg_j(a, "consent_check", consent);
    json_free(consent);
    json_t *sender = rm_call(io, "SharedMetricsSender", a);
    if (rm_is_raise(sender)) return sender;
    if (rm_is_null(sender)) {
        json_free(sender);
        return rm_mkraise("AttributeError", "'NoneType' object has no attribute 'send_pending'");
    }
    json_free(sender);
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("sender.send_pending"));
    json_t *ga = json_object();
    rm_arg_s(ga, "message", "Shared-metrics send pass failed");
    rm_arg_j(ga, "callback", cb);
    json_free(cb);
    json_set(ga, "args", json_array());
    json_set(ga, "kwargs", json_object());
    json_t *r = rm_call(io, "rt._guarded", ga);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 17. record_auxiliary_tokens — auxiliary token fields under the scope stack
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_record_auxiliary_tokens @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_auxiliary_tokens */
static json_t *rm_record_auxiliary_tokens(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *event = rm_get(in, "event");
    json_t *fa = json_object();
    rm_arg_j(fa, "usage", rm_get(event, "usage"));
    rm_arg_s(fa, "call_role", "auxiliary");
    rm_arg_j(fa, "aux_task", rm_get(event, "aux_task"));
    const json_t *model = rm_get(event, "response_model");
    if (!rm_truth(model)) model = rm_get(event, "model");
    rm_arg_j(fa, "model", model);
    rm_arg_j(fa, "provider", rm_get(event, "provider"));
    json_t *tokens = rm_call(io, "fields.model_token_fields", fa);
    if (rm_is_raise(tokens)) return tokens;
    if (rm_is_null(tokens)) { json_free(tokens); return NULL; }
    /* metadata kwarg evaluates BEFORE the _with_scope_stack seam fires */
    json_t *meta = rm_call(io, "rt._event_metadata", json_object());
    if (rm_is_raise(meta)) { json_free(tokens); return meta; }
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("relay.scope.event"));
    json_t *a = json_object();
    rm_arg_j(a, "callback", cb);
    json_free(cb);
    json_t *args_arr = json_array();
    json_append(args_arr, json_string("hermes.model_tokens"));
    json_set(a, "args", args_arr);
    json_t *kwargs_obj = json_object();
    rm_arg_j(kwargs_obj, "data", tokens);
    json_free(tokens);
    rm_arg_j(kwargs_obj, "metadata", meta);
    json_free(meta);
    json_set(a, "kwargs", kwargs_obj);
    json_t *r = rm_call(io, "rt._with_scope_stack", a);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 18. record_switch_after — turns-before-switch for a served session
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_record_switch_after @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_switch_after */
static json_t *rm_record_switch_after(rm_io_t *io, const json_t *in, json_t *st)
{
    const char *session_id = json_get_str(in, "session_id", "");
    json_t *event = json_object();
    json_set(event, "session_id", json_string(session_id));
    json_t *sa = json_object();
    rm_arg_j(sa, "event", event);
    json_free(event);
    json_t *session = rm_call(io, "rt._session", sa);
    if (rm_is_raise(session)) return session;
    const json_t *own = !rm_is_null(session) ? rm_get(session, "route_run") : NULL;
    json_t *taken = rm_with_route_run_core(io, st, session_id, own, NULL, true);
    json_free(session);
    if (rm_is_raise(taken)) return taken;
    if (rm_is_null(taken)) { json_free(taken); return NULL; }
    /* engagement_.switch_after_fields(*taken) — the splat evaluates first */
    json_t *ta = json_object();
    json_set(ta, "args", json_copy(taken));
    json_free(taken);
    json_t *fields = rm_call(io, "engagement.switch_after_fields", ta);
    if (rm_is_raise(fields)) return fields;
    json_t *ma = json_object();
    rm_arg_s(ma, "mark", "hermes.model_switch_after");
    rm_arg_j(ma, "data", fields);
    json_free(fields);
    json_t *r = rm_call(io, "rt.record_process_mark", ma);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 19. record_friction — friction signal + wasted-turn rows
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_record_friction @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_friction */
static json_t *rm_record_friction(rm_io_t *io, const json_t *in, json_t *st)
{
    const char *signal = json_get_str(in, "signal", "");
    const char *session_id = json_get_str(in, "session_id", "");
    const json_t *fallback_route = rm_get(in, "fallback_route");
    double turns = rm_num(in, "turns", 1.0);
    json_t *session = NULL;
    if (session_id[0]) {
        json_t *event = json_object();
        json_set(event, "session_id", json_string(session_id));
        json_t *sa = json_object();
        rm_arg_j(sa, "event", event);
        json_free(event);
        session = rm_call(io, "rt._session", sa);
        if (rm_is_raise(session)) return session;
    }
    const json_t *route = NULL;
    if (!rm_is_null(session)) {
        const json_t *model_state = rm_get(session, "model_state");
        route = rm_get(model_state, "last_route");
    }
    json_t *wasted = json_array();
    /* WASTE_REASONS = frozenset({"interrupt", "retry", "undo"}) */
    bool waste = rm_str_eq(signal, "interrupt") || rm_str_eq(signal, "retry") || rm_str_eq(signal, "undo");
    if (waste) {
        json_t *efficiency = NULL;   /* owned when freshly constructed */
        if (!rm_is_null(session)) {
            efficiency = rm_get(session, "efficiency");
        } else {
            efficiency = rm_call(io, "eff.SessionEfficiency", json_object());
            if (rm_is_raise(efficiency)) { json_free(session); json_free(wasted); return efficiency; }
            /* if session is None and (spent := _lineage_spent(sid)) is not None */
            json_t *la = json_object();
            rm_arg_s(la, "session_id", session_id);
            json_t *spent = rm_call(io, "rt._lineage_spent", la);
            if (rm_is_raise(spent)) { json_free(efficiency); json_free(session); json_free(wasted); return spent; }
            if (!rm_is_null(spent)) {
                if (rm_is_obj(efficiency)) json_set(efficiency, "spent", spent);
                else json_free(spent);
            } else {
                json_free(spent);
            }
        }
        const json_t *effective = rm_truth(route) ? route : fallback_route;
        json_t *da = json_object();
        rm_arg_s(da, "signal", signal);
        rm_arg_n(da, "turns", turns);
        rm_arg_j(da, "route", effective);
        json_t *d = rm_call(io, "efficiency.discard_turns", da);
        if (rm_is_raise(d)) {
            if (rm_is_null(session)) json_free(efficiency);
            json_free(session); json_free(wasted);
            return d;
        }
        json_free(wasted);
        wasted = rm_is_arr(d) ? d : NULL;   /* null result: rows kwarg dropped */
        if (!rm_is_arr(d)) json_free(d);
        if (rm_is_null(session)) json_free(efficiency);
    }
    const json_t *effective = rm_truth(route) ? route : fallback_route;
    json_t *fa = json_object();
    rm_arg_s(fa, "signal", signal);
    rm_arg_j(fa, "route", effective);
    json_t *data = rm_call(io, "model.friction_fields", fa);
    json_free(session);
    if (rm_is_raise(data)) { json_free(wasted); return data; }
    if (rm_is_null(data)) { json_free(data); json_free(wasted); return NULL; }
    json_t *ma = json_object();
    rm_arg_s(ma, "mark", "hermes.model_friction");
    rm_arg_j(ma, "data", data);
    json_free(data);
    json_t *r = rm_call(io, "rt.record_process_mark", ma);
    if (rm_is_raise(r)) { json_free(wasted); return r; }
    json_free(r);
    json_t *ea = json_object();
    rm_arg_j(ea, "rows", wasted);   /* session None dropped; null rows dropped */
    json_free(wasted);
    r = rm_call(io, "rt._emit_rows", ea);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 20. _reconcile_store_consent — nested CMs: store._connection / write_txn
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rm_reconcile_store_consent_core(rm_io_t *io, const json_t *store, bool send_enabled)
{
    (void)store;   /* receiver travels via the store._connection seam name */
    json_t *outer = rm_call(io, "store._connection", json_object());
    if (rm_is_raise(outer)) return outer;
    json_t *bad = rm_cm_check(outer);
    if (bad) { json_free(outer); return bad; }
    json_t *connection = rm_cm_enter(io, outer);
    if (rm_is_raise(connection)) { json_free(outer); return connection; }
    /* inner with write_txn(connection): reconcile_send_consent(...) */
    json_t *body_raise = NULL;
    json_t *inner = NULL;
    bool inner_entered = false;
    json_t *wa = json_object();
    rm_arg_j(wa, "connection", connection);
    inner = rm_call(io, "write_txn", wa);
    if (rm_is_raise(inner)) {
        body_raise = inner;
        inner = NULL;
    } else {
        bad = rm_cm_check(inner);
        if (bad) {
            body_raise = bad;
            json_free(inner);
            inner = NULL;
        } else {
            json_t *ienter = rm_cm_enter(io, inner);
            if (rm_is_raise(ienter)) {
                body_raise = ienter;
            } else {
                json_free(ienter);
                inner_entered = true;
                json_t *ra = json_object();
                rm_arg_j(ra, "connection", connection);
                rm_arg_b(ra, "send_enabled", send_enabled);
                json_t *r = rm_call(io, "reconcile_send_consent", ra);
                if (rm_is_raise(r)) body_raise = r; else json_free(r);
            }
        }
    }
    /* inner __exit__ runs only when the inner CM was actually entered */
    if (inner_entered) {
        const char *bk = body_raise ? rm_kind(body_raise) : NULL;
        json_t *ex = rm_cm_exit(io, inner, bk);
        if (rm_is_raise(ex)) {
            json_free(body_raise);
            body_raise = ex;
        } else {
            if (body_raise && rm_truth(ex)) { json_free(body_raise); body_raise = NULL; }
            json_free(ex);
        }
    }
    json_free(inner);
    json_free(connection);
    /* outer __exit__ */
    const char *bk = body_raise ? rm_kind(body_raise) : NULL;
    json_t *ex = rm_cm_exit(io, outer, bk);
    json_free(outer);
    if (rm_is_raise(ex)) { json_free(body_raise); return ex; }
    if (body_raise && rm_truth(ex)) { json_free(body_raise); json_free(ex); return NULL; }
    json_free(ex);
    if (body_raise) return body_raise;
    return NULL;
}
/* PoP: rm_reconcile_store_consent @ hermes_cli/observability/relay_shared_metrics.py:_reconcile_store_consent */
static json_t *rm_reconcile_store_consent(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return rm_reconcile_store_consent_core(io, rm_get(in, "store"),
                                           rm_bool(in, "send_enabled", false));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 21. _reconcile_send_consent_once — once-per-process gate; path probe;
 * store ctor; REAL _reconcile_store_consent; Exception log-and-swallow
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_reconcile_send_consent_once @ hermes_cli/observability/relay_shared_metrics.py:_reconcile_send_consent_once */
static json_t *rm_reconcile_send_consent_once(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    if (rm_truth(rm_get(st, "_consent_reconcile_done"))) return NULL;
    json_set(st, "_consent_reconcile_done", json_bool(true));
    json_t *resolved = NULL;
    json_t *home = NULL;
    char *path = NULL;
    json_t *store = NULL;
    json_t *r = NULL;
    /* try: */
    resolved = rm_call(io, "_resolved_send_config", json_object());
    if (rm_is_raise(resolved)) {
        if (rm_is_base(rm_kind(resolved))) return resolved;
        json_free(resolved); resolved = NULL;
        goto warn;
    }
    home = rm_call(io, "get_hermes_home", json_object());
    if (rm_is_raise(home)) {
        if (rm_is_base(rm_kind(home))) { json_free(resolved); return home; }
        json_free(home); home = NULL;
        json_free(resolved); resolved = NULL;
        goto warn;
    }
    if (!rm_is_str(home)) {
        /* live `home / "telemetry"` TypeError (null home domain) — swallowed */
        json_free(home); home = NULL;
        json_free(resolved); resolved = NULL;
        goto warn;
    }
    path = rm_posix_join(rm_str(home), "telemetry/shared_metrics/metrics.sqlite3");
    json_free(home); home = NULL;
    {
        /* if not resolved.send and not default_path.exists(): return */
        json_t *send = rm_read_attr(resolved, "send");
        if (rm_is_raise(send)) {
            if (rm_is_base(rm_kind(send))) { json_free(resolved); free(path); return send; }
            json_free(send);
            json_free(resolved); resolved = NULL;
            free(path); path = NULL;
            goto warn;
        }
        bool send_on = rm_truth(send);
        json_free(send);
        if (!send_on) {
            json_t *pa = json_object();
            rm_arg_s(pa, "path", path);
            json_t *exists = rm_call(io, "path.exists", pa);
            if (rm_is_raise(exists)) {
                if (rm_is_base(rm_kind(exists))) { json_free(resolved); free(path); return exists; }
                json_free(exists);
                json_free(resolved); resolved = NULL;
                free(path); path = NULL;
                goto warn;
            }
            bool has_store = rm_truth(exists);
            json_free(exists);
            if (!has_store) { json_free(resolved); free(path); return NULL; }
        }
    }
    free(path); path = NULL;
    store = rm_call(io, "SharedMetricsStore", json_object());
    if (rm_is_raise(store)) {
        if (rm_is_base(rm_kind(store))) { json_free(resolved); return store; }
        json_free(store); store = NULL;
        json_free(resolved); resolved = NULL;
        goto warn;
    }
    {
        json_t *send = rm_read_attr(resolved, "send");
        if (rm_is_raise(send)) {
            if (rm_is_base(rm_kind(send))) { json_free(store); json_free(resolved); return send; }
            json_free(send);
            json_free(store); store = NULL;
            json_free(resolved); resolved = NULL;
            goto warn;
        }
        bool send_on = rm_truth(send);
        json_free(send);
        r = rm_reconcile_store_consent_core(io, store, send_on);
        json_free(store); store = NULL;
        json_free(resolved); resolved = NULL;
        if (rm_is_raise(r)) {
            if (rm_is_base(rm_kind(r))) return r;
            json_free(r); r = NULL;
            goto warn;
        }
        json_free(r);
    }
    return NULL;
warn:
    {
        json_t *lr = rm_log(io, "warning", "Unable to reconcile shared-metrics send consent", true);
        if (rm_is_raise(lr)) return lr;
        json_free(lr);
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 22. record_session_friction (module) — instrumentation gate; _get_runtime
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_record_session_friction @ hermes_cli/observability/relay_shared_metrics.py:record_session_friction */
static json_t *rm_record_session_friction(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *ie = rm_call(io, "relay_runtime.relay_instrumentation_enabled", json_object());
    if (rm_is_raise(ie)) return ie;
    bool on = rm_truth(ie);
    json_free(ie);
    if (!on) return NULL;
    json_t *ga = json_object();
    rm_arg_b(ga, "retry_failed", true);
    json_t *runtime = rm_call(io, "_get_runtime", ga);
    if (rm_is_raise(runtime)) return runtime;
    if (rm_is_null(runtime)) { json_free(runtime); return NULL; }
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(json_get_str(in, "signal", "")));
    json_append(args_arr, json_string(json_get_str(in, "session_id", "")));
    json_append(args_arr, rm_copy_or_null(rm_get(in, "fallback_route")));
    json_append(args_arr, json_number(rm_num(in, "turns", 1.0)));
    json_t *r = rm_rt_safe(io, runtime, "rt.record_friction", args_arr, json_object());
    json_free(runtime);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 23. record_session_tools (module) — registry lookup; never creates a runtime
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_record_session_tools @ hermes_cli/observability/relay_shared_metrics.py:record_session_tools */
static json_t *rm_record_session_tools(rm_io_t *io, const json_t *in, json_t *st)
{
    json_t *pk = rm_call(io, "relay_runtime.current_profile_key", json_object());
    if (rm_is_raise(pk)) return pk;
    const char *key = rm_str(pk);
    const json_t *registry = rm_get(st, "_RUNTIMES");
    const json_t *runtime = rm_get(registry, key ? key : "");
    json_free(pk);
    if (!rm_is_runtime(runtime)) return NULL;
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(json_get_str(in, "session_id", "")));
    json_append(args_arr, rm_copy_or_null(rm_get(in, "agent")));
    json_append(args_arr, rm_copy_or_null(rm_get(in, "tools_for_api")));
    json_t *r = rm_rt_safe(io, runtime, "rt.record_session_tools", args_arr, json_object());
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 24. record_known_cache_break (module) — instrumentation gate; _get_runtime
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_record_known_cache_break @ hermes_cli/observability/relay_shared_metrics.py:record_known_cache_break */
static json_t *rm_record_known_cache_break(rm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *ie = rm_call(io, "relay_runtime.relay_instrumentation_enabled", json_object());
    if (rm_is_raise(ie)) return ie;
    bool on = rm_truth(ie);
    json_free(ie);
    if (!on) return NULL;
    json_t *ga = json_object();
    rm_arg_b(ga, "retry_failed", true);
    json_t *runtime = rm_call(io, "_get_runtime", ga);
    if (rm_is_raise(runtime)) return runtime;
    if (rm_is_null(runtime)) { json_free(runtime); return NULL; }
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(json_get_str(in, "cause", "")));
    json_append(args_arr, rm_copy_or_null(rm_get(in, "route")));
    json_append(args_arr, json_string(json_get_str(in, "session_id", "")));
    json_t *r = rm_rt_safe(io, runtime, "rt.record_known_cache_break", args_arr, json_object());
    json_free(runtime);
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 25. record_model_switch_after (module) — id + enabled + instrumentation
 * gates; registry lookup; never creates a runtime
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm_record_model_switch_after @ hermes_cli/observability/relay_shared_metrics.py:record_model_switch_after */
static json_t *rm_record_model_switch_after(rm_io_t *io, const json_t *in, json_t *st)
{
    const char *session_id = json_get_str(in, "session_id", "");
    if (!session_id[0]) return NULL;
    json_t *en = rm_call(io, "enabled", json_object());
    if (rm_is_raise(en)) return en;
    bool enabled = rm_truth(en);
    json_free(en);
    if (!enabled) return NULL;
    json_t *ie = rm_call(io, "relay_runtime.relay_instrumentation_enabled", json_object());
    if (rm_is_raise(ie)) return ie;
    bool on = rm_truth(ie);
    json_free(ie);
    if (!on) return NULL;
    json_t *pk = rm_call(io, "relay_runtime.current_profile_key", json_object());
    if (rm_is_raise(pk)) return pk;
    const char *key = rm_str(pk);
    const json_t *registry = rm_get(st, "_RUNTIMES");
    const json_t *runtime = rm_get(registry, key ? key : "");
    json_free(pk);
    if (!rm_is_runtime(runtime)) return NULL;
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(session_id));
    json_t *r = rm_rt_safe(io, runtime, "rt.record_switch_after", args_arr, json_object());
    if (rm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}