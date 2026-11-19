/* relay_protocol_flow.c — FLOW port of the UNCLAIMED remainder of the relay
 * wire protocol. The task names gateway/relay/protocol.py, which does NOT
 * exist in the quarry: gateway/relay/transport.py is the pure Protocol surface
 * (every method body is `...`), and the REAL defining module of the relay
 * protocol behaviour is gateway/relay/ws_transport.py (the newline-delimited
 * JSON frame protocol implementation). PoP credit therefore goes to
 * gateway/relay/ws_transport.py:<fn> (the defining module).
 *
 * 22 ported defs (claimed siblings SEAMED through the universal rp_io_t seam):
 *   module leaves : _per_url_from_wire, _media_types_from_wire,
 *                   _media_filenames_from_wire, _await_bounded
 *   _PendingRequest.__init__ (super().__init__ seamed)
 *   WebSocketRelayTransport.__init__ (_ws_dial_url + asyncio.Event seamed)
 *   methods       : _fail_pending, _settle_connection_lost, _dialer_running,
 *                   _close_reason_of, _latch_if_fresh_token_refused,
 *                   hold_redial, release_redial, _await_redial_hold,
 *                   _redial_with_fresh_token, _on_descriptor, _on_inbound,
 *                   _ack_admitted, _on_going_idle_ack, _on_outbound_result,
 *                   _on_interrupt_inbound, _on_passthrough_forward
 *
 * Ported functions call each other REAL (internal composition):
 *   _media_types_from_wire / _media_filenames_from_wire -> _per_url_from_wire
 *   _redial_with_fresh_token -> _latch_if_fresh_token_refused
 *   _latch_if_fresh_token_refused -> _close_reason_of
 *   _fail_pending -> _settle_connection_lost (when settle == {"__self__": ...})
 *
 * Seamed claimed siblings / foreign leaves (NO PoP re-claim):
 *   _ws_dial_url, _event_from_wire, _passthrough_from_wire, _dial_and_start,
 *   _reconnect_loop, _send_inbound_ack, _close_code_of (ws_transport.py),
 *   clean_attachment_filename (gateway/platforms/event.py),
 *   CapabilityDescriptor.from_json (gateway/relay/descriptor.py),
 *   json.dumps, asyncio.Event / asyncio.create_task / asyncio.wait_for,
 *   relay_callback (gateway/session_ingress_context.py, in-function import),
 *   logger.*, and the future/task/event double methods.
 *
 * Representation contract (shared with tests/oracle_relay_protocol_flow.py):
 *   - raise specs are VALUES {"__raise__": kind, "message": msg, ...attrs}
 *     threaded through every helper; an uncaught one becomes the case ret.
 *     The BaseException trio (SystemExit / KeyboardInterrupt / GeneratorExit)
 *     plus asyncio.CancelledError passes through every `except Exception`
 *     guard. _await_bounded's `except (TimeoutError, CancelledError, Exception)`
 *     ALSO swallows CancelledError — only the trio passes there.
 *   - fail-closed: an absent / exhausted seam script returns a RuntimeError
 *     raise spec ("no script for <name>"), recorded as the event ret BEFORE
 *     propagation.
 *   - asyncio doubles: future {"__future__":true,id,done,result,exception,
 *     cancelled,op?}; task {"__task__":true,id,done,cancelled,exception};
 *     event {"__event__":true,id,set}. set_result / add_done_callback /
 *     event.set / event.clear / event.wait fire seams and mutate the double.
 *     done()/cancelled()/exception() are plain reads (no seam).
 *   - callable doubles {"__callable__": seam} fire that seam with {args,kwargs}.
 *     relay_callback is a silent context manager: the call is the seam boundary;
 *     __enter__/__exit__ record nothing (the live CM only sets/resets a ContextVar).
 *   - scripts: a bare JSON array is a per-call QUEUE; a list RESULT must be
 *     wrapped {"__script__": true, "ret": [...]}. A scripted null returns None.
 *     None-valued seam-arg OBJECT keys are dropped on both sides (array
 *     elements kept).
 *   - state window (st): a copy of in.state with per-func __init__ defaults
 *     materialized when absent, mutated in place. __init__ builds st fresh.
 *     _inbound_acks is a set of task ids, kept SORTED on both sides.
 *   - fixture domain (ASCII scope): str() operands are ASCII scalars; raw /
 *     frame / descriptor maps are dict-or-falsy; media_urls / media / identities
 *     are list-or-falsy; exc inputs are raise specs (attrs materialized as
 *     attribute doubles); platform dict-key positions are str-or-falsy.
 *     EXCLUDED: integral-FLOAT literals in str()/type-name positions (libjson
 *     has no int/float identity); KeyError raise specs in exc-arg positions
 *     (str(KeyError) is repr-shaped, not the message); non-str truthy
 *     descriptor.platform keys; non-list truthy identities.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "gateway/relay_protocol_flow.c". rp_ prefix.
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
typedef struct rp_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} rp_io_t;

/* ── truthiness / accessors ── */
static bool rp_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool rp_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rp_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool rp_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool rp_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool rp_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool rp_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}

static const char *rp_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *rp_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static bool rp_bool(const json_t *o, const char *k, bool def)
{
    const json_t *v = rp_get(o, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : def;
}

static bool rp_str_eq(const char *a, const char *b)
{
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static json_t *rp_copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

static const char *rp_type_name(const json_t *v)
{
    if (rp_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* live .get(k, default): absent -> default; present (even null) -> value. */
static const json_t *rp_get_def(const json_t *o, const char *k, const json_t *def)
{
    const json_t *v = rp_get(o, k);
    return v ? v : def;
}

/* ── raise specs + the FULL builtin exception hierarchy ── */
static bool rp_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *rp_kind(const json_t *r)
{
    const char *k = rp_str(rp_get(r, "__raise__"));
    return k ? k : "";
}
static const char *rp_msg(const json_t *r)
{
    const char *m = rp_str(rp_get(r, "message"));
    return m ? m : "";
}
static json_t *rp_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *rp_xfmt(const char *fmt, ...)
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

/* parent table: every builtin Exception subclass walks to Exception; the
 * BaseException trio (and CancelledError) walk to BaseException ONLY. */
typedef struct { const char *child; const char *parent; } rp_exc_rel_t;
static const rp_exc_rel_t RP_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"TypeError", "Exception"},
    {"AttributeError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"ArithmeticError", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"FloatingPointError", "ArithmeticError"},
    {"OSError", "Exception"},
    {"IOError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FileExistsError", "OSError"},
    {"PermissionError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"InterruptedError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"AssertionError", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"MemoryError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ReferenceError", "Exception"},
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
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"ExceptionGroup", "Exception"},
    {"BaseExceptionGroup", "BaseException"},
};
static const char *rp_exc_parent(const char *kind)
{
    for (size_t i = 0; i < sizeof(RP_EXC_PARENTS) / sizeof(RP_EXC_PARENTS[0]); i++)
        if (rp_str_eq(RP_EXC_PARENTS[i].child, kind))
            return RP_EXC_PARENTS[i].parent;
    /* unknown scripted kind: the oracle builds type(kind, (Exception,), {}) */
    return "Exception";
}
static bool rp_is_exception(const char *kind)
{
    const char *k = kind;
    for (int depth = 0; k && depth < 16; depth++) {
        if (rp_str_eq(k, "Exception")) return true;
        if (rp_str_eq(k, "BaseException")) return false;
        k = rp_exc_parent(k);
    }
    return false;
}
static bool rp_is_base(const char *kind)
{
    return rp_str_eq(kind, "SystemExit") || rp_str_eq(kind, "KeyboardInterrupt")
        || rp_str_eq(kind, "GeneratorExit") || rp_str_eq(kind, "CancelledError");
}
/* true when a raise spec must pass through an `except Exception` guard. */
static bool rp_passthrough(const json_t *r)
{
    return rp_is_base(rp_kind(r)) || !rp_is_exception(rp_kind(r));
}
/* the trio ONLY (CancelledError is caught by _await_bounded's except tuple). */
static bool rp_trio(const json_t *r)
{
    const char *k = rp_kind(r);
    return rp_str_eq(k, "SystemExit") || rp_str_eq(k, "KeyboardInterrupt")
        || rp_str_eq(k, "GeneratorExit");
}
/* kind walks to TimeoutError (except TimeoutError). */
static bool rp_is_timeout(const json_t *r)
{
    const char *k = rp_kind(r);
    for (int depth = 0; k && depth < 16; depth++) {
        if (rp_str_eq(k, "TimeoutError")) return true;
        if (rp_str_eq(k, "BaseException")) return false;
        k = rp_exc_parent(k);
    }
    return false;
}

/* ── universal seam call: args OWNED by caller, freed here; result caller-owned
 * (NULL == None, or a raise spec). Fail-closed when no channel installed. ── */
static json_t *rp_call(rp_io_t *io, const char *name, json_t *args)
{
    if (!io || !io->call) {
        char *m = rp_xfmt("no seam installed for %s", name);
        json_t *r = rp_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* arg builders: None-valued keys dropped on both sides ("" kept). */
static void rp_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void rp_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void rp_arg_b(json_t *a, const char *k, bool v)
{
    json_set(a, k, json_bool(v));
}

/* logger seam: {msg, args:[...], exc_info?}. args_arr ownership consumed.
 * exc_info: NULL omit; json_bool(true) -> true; raise spec -> the exc value. */
static json_t *rp_log_arr(rp_io_t *io, const char *level, const char *msg,
                          json_t *args_arr, const json_t *exc_info)
{
    json_t *a = json_object();
    rp_arg_s(a, "msg", msg);
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) {
        if (exc_info->type == JSON_BOOL) rp_arg_b(a, "exc_info", exc_info->bool_val);
        else json_set(a, "exc_info", json_copy(exc_info));
    }
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    return rp_call(io, name, a);
}
static json_t *rp_log(rp_io_t *io, const char *level, const char *msg)
{
    return rp_log_arr(io, level, msg, json_array(), NULL);
}

/* invoke a callable double {"__callable__": seam}; args/kwargs consumed. */
static json_t *rp_invoke_callable(rp_io_t *io, const json_t *cb,
                                  json_t *args_arr, json_t *kwargs_obj)
{
    const char *name = rp_str(rp_get(cb, "__callable__"));
    if (!name) {
        json_free(args_arr);
        json_free(kwargs_obj);
        char *m = rp_xfmt("'%s' object is not callable", rp_type_name(cb));
        json_t *r = rp_mkraise("TypeError", m ? m : "object is not callable");
        free(m);
        return r;
    }
    json_t *a = json_object();
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
    return rp_call(io, name, a);
}

/* ── str() leaf (ASCII scalar domain) ── */
static void rp_float_repr(double v, char *out, size_t cap)
{
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}
static char *rp_pystr(const json_t *v)
{
    if (rp_is_null(v)) return strdup("None");
    if (rp_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (rp_is_num(v)) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            rp_float_repr(d, buf, sizeof buf);
        return strdup(buf);
    }
    if (rp_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return json_serialize(v);   /* containers: outside the str() fixture domain */
}

/* ── state map ensure (mirrors __init__ default {}; mutates st in place) ── */
static json_t *rp_ensure_map(json_t *st, const char *key)
{
    json_t *m = json_obj_get(st, key);
    if (!m || m->type != JSON_OBJECT) {
        m = json_object();
        json_set(st, key, m);
    }
    return m;
}

/* ── double helpers ── */
static bool rp_done(const json_t *fut) { return rp_bool(fut, "done", false); }

/* future.set_result: seam {id, result}; on success mutate done/result.
 * Returns the seam result (raise spec or NULL). result borrowed. */
static json_t *rp_future_set_result(rp_io_t *io, json_t *fut, const json_t *result)
{
    json_t *a = json_object();
    rp_arg_j(a, "id", rp_get(fut, "id"));
    rp_arg_j(a, "result", result);
    json_t *r = rp_call(io, "future.set_result", a);
    if (rp_is_raise(r)) return r;
    json_free(r);
    json_set(fut, "done", json_bool(true));
    json_set(fut, "result", rp_copy_or_null(result));
    return NULL;
}

/* task.add_done_callback: seam {id, cb:{"__callable__":name}}. */
static json_t *rp_task_add_done_callback(rp_io_t *io, const json_t *task, const char *cbname)
{
    json_t *a = json_object();
    rp_arg_j(a, "id", rp_get(task, "id"));
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string(cbname));
    json_set(a, "cb", cb);
    return rp_call(io, "task.add_done_callback", a);
}

/* event.set / event.clear: seam {id}; mutate the double's set flag.
 * event.wait: seam {id}; no mutation. Returns seam result. */
static json_t *rp_event_op(rp_io_t *io, json_t *ev, const char *op)
{
    json_t *a = json_object();
    rp_arg_j(a, "id", rp_get(ev, "id"));
    char name[24];
    snprintf(name, sizeof name, "event.%s", op);
    json_t *r = rp_call(io, name, a);
    if (rp_is_raise(r)) return r;
    json_free(r);
    if (rp_str_eq(op, "set")) json_set(ev, "set", json_bool(true));
    else if (rp_str_eq(op, "clear")) json_set(ev, "set", json_bool(false));
    return NULL;
}

/* AttributeError helper for a missing attribute on a non-carrier. */
static json_t *rp_attr_raise(const json_t *v, const char *attr)
{
    char *m = rp_xfmt("'%s' object has no attribute '%s'", rp_type_name(v), attr);
    json_t *r = rp_mkraise("AttributeError", m ? m : "");
    free(m);
    return r;
}

/* ════════════════════════ ported cores ════════════════════════ */

/* ── _per_url_from_wire ── */
/* PoP: rp_per_url_from_wire @ gateway/relay/ws_transport.py:_per_url_from_wire */
static json_t *rp_per_url_from_wire(rp_io_t *io, const json_t *raw, const char *key)
{
    (void)io;
    if (!rp_is_obj(raw)) return rp_attr_raise(raw, "get");
    const json_t *urls = rp_get(raw, "media_urls");
    if (!rp_is_arr(urls) || urls->c.count == 0) return json_array();
    const json_t *media = rp_get(raw, "media");
    json_t *by_url = json_object();
    if (rp_is_arr(media)) {
        for (size_t i = 0; i < media->c.count; i++) {
            const json_t *m = media->c.items[i];
            if (!rp_is_obj(m)) continue;
            const json_t *url = rp_get(m, "url");
            if (!rp_is_str(url) || !url->str_val || !url->str_val[0]) continue;
            const json_t *v = rp_get(m, key);
            json_set(by_url, url->str_val, rp_is_str(v) ? json_copy(v) : json_string(""));
        }
    }
    json_t *out = json_array();
    for (size_t i = 0; i < urls->c.count; i++) {
        const json_t *u = urls->c.items[i];
        const json_t *e = rp_is_str(u) ? json_obj_get(by_url, u->str_val) : NULL;
        json_append(out, rp_is_str(e) ? json_copy(e) : json_string(""));
    }
    json_free(by_url);
    return out;
}

/* ── _media_types_from_wire ── */
/* PoP: rp_media_types_from_wire @ gateway/relay/ws_transport.py:_media_types_from_wire */
static json_t *rp_media_types_from_wire(rp_io_t *io, const json_t *raw)
{
    json_t *types = rp_per_url_from_wire(io, raw, "mime");
    if (rp_is_raise(types)) return types;
    size_t missing = 0;
    for (size_t i = 0; i < types->c.count; i++) {
        const json_t *t = types->c.items[i];
        if (!rp_truth(t)) missing++;
    }
    const json_t *media = rp_get(raw, "media");
    if (missing > 0 && rp_is_arr(media) && media->c.count > 0) {
        json_t *args = json_array();
        json_append(args, json_number((double)missing));
        json_append(args, json_number((double)types->c.count));
        json_t *lr = rp_log_arr(io, "debug",
            "relay inbound: %d/%d media_urls had no matching media[] mime", args, NULL);
        if (rp_is_raise(lr)) { json_free(types); return lr; }
        json_free(lr);
    }
    return types;
}

/* ── _media_filenames_from_wire ── */
/* PoP: rp_media_filenames_from_wire @ gateway/relay/ws_transport.py:_media_filenames_from_wire */
static json_t *rp_media_filenames_from_wire(rp_io_t *io, const json_t *raw)
{
    json_t *names = rp_per_url_from_wire(io, raw, "filename");
    if (rp_is_raise(names)) return names;
    /* comprehension is ATOMIC: a raise discards all partial results. */
    json_t *out = json_array();
    for (size_t i = 0; i < names->c.count; i++) {
        json_t *a = json_object();
        rp_arg_j(a, "n", names->c.items[i]);
        json_t *r = rp_call(io, "clean_attachment_filename", a);
        if (rp_is_raise(r)) { json_free(out); json_free(names); return r; }
        json_append(out, r ? r : json_null());
    }
    json_free(names);
    return out;
}

/* ── _await_bounded ── */
/* PoP: rp_await_bounded @ gateway/relay/ws_transport.py:_await_bounded */
static json_t *rp_await_bounded(rp_io_t *io, const json_t *aw)
{
    /* except (TimeoutError, asyncio.CancelledError, Exception): pass —
     * swallows everything EXCEPT the BaseException trio. */
    json_t *a = json_object();
    rp_arg_j(a, "aw", aw);
    json_set(a, "timeout", json_number(1.0));   /* _TEARDOWN_AWAIT_TIMEOUT_S */
    json_t *r = rp_call(io, "asyncio.wait_for", a);
    if (rp_is_raise(r)) {
        if (rp_trio(r)) return r;
        json_free(r);
    } else {
        json_free(r);
    }
    return json_null();
}

/* ── _PendingRequest.__init__ ── */
/* PoP: rp_pending_request_init @ gateway/relay/ws_transport.py:_PendingRequest.__init__ */
static json_t *rp_pending_request_init(rp_io_t *io, const json_t *op, const json_t *loop, json_t *st)
{
    json_t *a = json_object();
    rp_arg_j(a, "loop", loop);
    json_t *r = rp_call(io, "super.__init__", a);
    if (rp_is_raise(r)) return r;
    json_free(r);
    json_set(st, "op", rp_copy_or_null(op));
    return json_null();
}

/* ── WebSocketRelayTransport.__init__ ── */
/* PoP: rp_wst_init @ gateway/relay/ws_transport.py:WebSocketRelayTransport.__init__ */
static json_t *rp_wst_init(rp_io_t *io, const json_t *in, json_t *st)
{
    if (!rp_bool(in, "websockets_available", true)) {
        return rp_mkraise("RuntimeError",
            "WebSocketRelayTransport requires the 'websockets' package "
            "(a hermes-agent core dependency).");
    }
    const json_t *url = rp_get(in, "url");
    const json_t *platform = rp_get(in, "platform");
    const json_t *bot_id = rp_get(in, "bot_id");
    const json_t *identities = rp_get(in, "identities");

    /* self._url = _ws_dial_url(url) */
    json_t *a = json_object();
    rp_arg_j(a, "url", url);
    json_t *u = rp_call(io, "_ws_dial_url", a);
    if (rp_is_raise(u)) return u;
    json_set(st, "_url", u ? u : json_null());
    json_set(st, "_platform", rp_copy_or_null(platform));
    json_set(st, "_bot_id", rp_copy_or_null(bot_id));
    /* self._identities = list(identities) if identities else [(platform, bot_id)] */
    if (rp_truth(identities)) {
        json_set(st, "_identities", json_copy(identities));
    } else {
        json_t *ids = json_array();
        json_t *pair = json_array();
        json_append(pair, rp_copy_or_null(platform));
        json_append(pair, rp_copy_or_null(bot_id));
        json_append(ids, pair);
        json_set(st, "_identities", ids);
    }
    const json_t *cts = rp_get(in, "connect_timeout_s");
    json_set(st, "_connect_timeout_s", cts ? json_copy(cts) : json_number(30.0));
    const json_t *ots = rp_get(in, "outbound_timeout_s");
    json_set(st, "_outbound_timeout_s", ots ? json_copy(ots) : json_number(30.0));
    json_set(st, "_gateway_id", rp_copy_or_null(rp_get(in, "gateway_id")));
    json_set(st, "_upgrade_secret", rp_copy_or_null(rp_get(in, "upgrade_secret")));
    json_set(st, "_reconnect", rp_get(in, "reconnect") ? json_copy(rp_get(in, "reconnect")) : json_bool(false));
    json_set(st, "_reconnect_backoff_s", rp_get(in, "reconnect_backoff_s") ? json_copy(rp_get(in, "reconnect_backoff_s")) : json_number(1.0));
    json_set(st, "_reconnect_max_backoff_s", rp_get(in, "reconnect_max_backoff_s") ? json_copy(rp_get(in, "reconnect_max_backoff_s")) : json_number(30.0));
    json_set(st, "_supervisor", json_null());
    json_set(st, "_dormant", json_bool(false));
    json_set(st, "_dormant_redial_s", json_number(1.0));
    json_set(st, "_redial_held", json_bool(false));
    /* self._redial_release = asyncio.Event() */
    json_t *ev = rp_call(io, "asyncio.Event", json_object());
    if (rp_is_raise(ev)) return ev;
    json_set(st, "_redial_release", ev ? ev : json_null());
    json_set(st, "_redial_hold_max_s", json_number(60.0));   /* REDIAL_HOLD_MAX_S */
    json_set(st, "_ws", json_null());
    json_set(st, "_reader", json_null());
    json_set(st, "_inbound", json_null());
    json_set(st, "_inbound_acks", json_array());
    json_set(st, "_interrupt_inbound_handler", json_null());
    json_set(st, "_passthrough_handler", json_null());
    json_set(st, "_descriptor", json_null());
    json_set(st, "_descriptors_by_platform", json_object());
    json_set(st, "_descriptor_ready", json_null());
    json_set(st, "_pending", json_object());
    json_set(st, "_going_idle_ack", json_null());
    json_set(st, "_closing", json_bool(false));
    json_set(st, "_handshake_succeeded", json_bool(false));
    json_set(st, "_auth_revoked", json_bool(false));
    json_set(st, "_auth_retry", json_null());
    json_set(st, "_dial_generation", json_number(0));
    json_set(st, "_auth_retry_generation", json_null());
    json_set(st, "_auth_retry_pending", json_bool(false));
    return json_null();
}

/* ── _fail_pending ── */
/* PoP: rp_fail_pending @ gateway/relay/ws_transport.py:_fail_pending */
static json_t *rp_settle_connection_lost(rp_io_t *io, json_t *fut, json_t *st);
static json_t *rp_fail_pending(rp_io_t *io, const json_t *settle, json_t *st)
{
    json_t *pending = rp_ensure_map(st, "_pending");
    /* list() snapshot: settling wakes waiters whose finally-pop mutates the dict. */
    size_t n = pending->c.count;
    json_t **snap = (json_t **)malloc((n ? n : 1) * sizeof(json_t *));
    if (!snap) return rp_mkraise("MemoryError", "snapshot alloc");
    for (size_t i = 0; i < n; i++) snap[i] = pending->c.items[i];
    json_t *ret = NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *fut = snap[i];
        if (rp_done(fut)) continue;
        json_t *r;
        const char *self_nm = rp_str(rp_get(settle, "__self__"));
        if (self_nm && rp_str_eq(self_nm, "_settle_connection_lost")) {
            r = rp_settle_connection_lost(io, fut, st);
        } else {
            json_t *args = json_array();
            json_append(args, json_copy(fut));
            r = rp_invoke_callable(io, settle, args, json_object());
        }
        if (rp_is_raise(r)) { ret = r; break; }   /* _pending.clear() does NOT run */
        json_free(r);
    }
    free(snap);
    if (ret) return ret;
    /* self._pending.clear() */
    while (pending->c.count > 0) json_obj_del(pending, pending->c.keys[0]);
    return json_null();
}

/* ── _settle_connection_lost (staticmethod) ── */
/* PoP: rp_settle_connection_lost @ gateway/relay/ws_transport.py:_settle_connection_lost */
static json_t *rp_settle_connection_lost(rp_io_t *io, json_t *fut, json_t *st)
{
    (void)st;
    json_t *result = json_object();
    json_set(result, "success", json_bool(false));
    json_set(result, "error", json_string("relay transport connection lost"));
    const json_t *op = rp_get(fut, "op");
    if (rp_is_str(op) && rp_str_eq(op->str_val, "send_media"))
        json_set(result, "ambiguous", json_bool(true));
    if (!rp_is_obj(fut)) { json_free(result); return rp_attr_raise(fut, "set_result"); }
    json_t *r = rp_future_set_result(io, fut, result);
    json_free(result);
    if (rp_is_raise(r)) return r;
    return json_null();
}

/* ── _dialer_running ── */
/* PoP: rp_dialer_running @ gateway/relay/ws_transport.py:_dialer_running */
static json_t *rp_dialer_running(rp_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    const json_t *tasks[2];
    tasks[0] = rp_get(st, "_supervisor");
    tasks[1] = rp_get(st, "_auth_retry");   /* getattr default None when absent */
    for (int i = 0; i < 2; i++) {
        const json_t *t = tasks[i];
        if (!rp_is_null(t) && !rp_done(t)) return json_bool(true);
    }
    return json_bool(false);
}

/* ── _close_reason_of (staticmethod) ── */
/* PoP: rp_close_reason_of @ gateway/relay/ws_transport.py:_close_reason_of */
static json_t *rp_close_reason_of(rp_io_t *io, const json_t *exc)
{
    (void)io;
    static const char *const attrs[2] = { "rcvd", "sent" };
    for (int i = 0; i < 2; i++) {
        const json_t *frame = rp_get(exc, attrs[i]);
        const json_t *reason = rp_is_obj(frame) ? rp_get(frame, "reason") : NULL;
        if (rp_is_str(reason)) return json_copy(reason);
    }
    const json_t *reason = rp_get(exc, "reason");
    return rp_is_str(reason) ? json_copy(reason) : json_null();
}

/* ── _latch_if_fresh_token_refused ── */
/* PoP: rp_latch_if_fresh_token_refused @ gateway/relay/ws_transport.py:_latch_if_fresh_token_refused */
static json_t *rp_latch_if_fresh_token_refused(rp_io_t *io, const json_t *exc, json_t *st)
{
    if (!rp_bool(st, "_auth_retry_pending", false)) return json_bool(false);
    json_t *a = json_object();
    rp_arg_j(a, "exc", exc);
    json_t *code = rp_call(io, "_close_code_of", a);
    if (rp_is_raise(code)) return code;
    bool is4401 = rp_is_num(code) && code->num_val == 4401.0;
    json_free(code);
    if (!is4401) return json_bool(false);
    json_set(st, "_auth_retry_pending", json_bool(false));
    json_t *reason = rp_close_reason_of(io, exc);
    bool expired = rp_is_str(reason) && rp_str_eq(reason->str_val, "expired");
    json_free(reason);
    if (expired) return json_bool(false);
    json_set(st, "_auth_revoked", json_bool(true));
    json_t *lr = rp_log(io, "warning",
        "relay ws fresh-token re-dial rejected with 4401 \xe2\x80\x94 treating as a "
        "revoked relay credential (opt-out); not reconnecting");
    if (rp_is_raise(lr)) return lr;
    json_free(lr);
    return json_bool(true);
}

/* ── hold_redial ── */
/* PoP: rp_hold_redial @ gateway/relay/ws_transport.py:hold_redial */
static json_t *rp_hold_redial(rp_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *ev = rp_get(st, "_redial_release");
    json_t *r = rp_event_op(io, ev, "clear");
    if (rp_is_raise(r)) return r;
    json_set(st, "_redial_held", json_bool(true));
    return json_null();
}

/* ── release_redial ── */
/* PoP: rp_release_redial @ gateway/relay/ws_transport.py:release_redial */
static json_t *rp_release_redial(rp_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_set(st, "_redial_held", json_bool(false));
    json_t *ev = rp_get(st, "_redial_release");
    json_t *r = rp_event_op(io, ev, "set");
    if (rp_is_raise(r)) return r;
    return json_null();
}

/* ── _await_redial_hold ── */
/* PoP: rp_await_redial_hold @ gateway/relay/ws_transport.py:_await_redial_hold */
static json_t *rp_await_redial_hold(rp_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    if (!rp_bool(st, "_redial_held", false)) return json_null();
    json_t *ev = rp_get(st, "_redial_release");
    json_t *pending_raise = NULL;
    /* self._redial_release.wait() is the ARGUMENT — fires before wait_for. */
    json_t *wr = rp_event_op(io, ev, "wait");
    if (rp_is_raise(wr)) {
        if (rp_is_timeout(wr)) {
            json_t *lr = rp_log(io, "info", "relay: brokered suspend did not land, reconnecting");
            if (rp_is_raise(lr)) pending_raise = lr; else json_free(lr);
            json_free(wr);
        } else {
            pending_raise = wr;
        }
    } else {
        json_free(wr);
        json_t *a = json_object();
        json_t *aw = json_object();
        json_set(aw, "__event_wait__", rp_copy_or_null(rp_get(ev, "id")));
        json_set(a, "aw", aw);
        rp_arg_j(a, "timeout", rp_get(st, "_redial_hold_max_s"));
        json_t *r = rp_call(io, "asyncio.wait_for", a);
        if (rp_is_raise(r)) {
            if (rp_is_timeout(r)) {
                json_t *lr = rp_log(io, "info", "relay: brokered suspend did not land, reconnecting");
                if (rp_is_raise(lr)) pending_raise = lr; else json_free(lr);
                json_free(r);
            } else {
                pending_raise = r;
            }
        } else {
            json_free(r);
        }
    }
    /* finally: held = False; release.clear() — runs on EVERY exit path. */
    json_set(st, "_redial_held", json_bool(false));
    json_t *cr = rp_event_op(io, ev, "clear");
    if (rp_is_raise(cr)) { json_free(pending_raise); return cr; }
    if (pending_raise) return pending_raise;
    return json_null();
}

/* ── _redial_with_fresh_token ── */
/* PoP: rp_redial_with_fresh_token @ gateway/relay/ws_transport.py:_redial_with_fresh_token */
static json_t *rp_redial_with_fresh_token(rp_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    if (rp_bool(st, "_closing", false)) return json_null();
    json_t *r = rp_call(io, "_dial_and_start", json_object());
    if (!rp_is_raise(r)) {
        json_free(r);
        /* logger.info is INSIDE the try: its raise falls into except Exception. */
        json_t *lr = rp_log(io, "info", "relay ws re-dialed with a fresh upgrade token after 4401");
        if (!rp_is_raise(lr)) { json_free(lr); return json_null(); }
        r = lr;
    }
    /* except asyncio.CancelledError: raise */
    if (rp_str_eq(rp_kind(r), "CancelledError")) return r;
    /* except Exception: (BaseException trio passes through) */
    if (rp_passthrough(r)) return r;
    if (rp_bool(st, "_closing", false)) { json_free(r); return json_null(); }
    json_t *latch = rp_latch_if_fresh_token_refused(io, r, st);
    if (rp_is_raise(latch)) { json_free(r); return latch; }
    bool latched = rp_truth(latch);
    json_free(latch);
    if (latched) { json_free(r); return json_null(); }
    json_t *args = json_array();
    json_append(args, json_string(rp_msg(r)));
    json_t *lr = rp_log_arr(io, "warning", "relay ws fresh-token re-dial failed: %s", args, NULL);
    if (rp_is_raise(lr)) { json_free(r); return lr; }
    json_free(lr);
    /* if self._reconnect and not (self._supervisor and not self._supervisor.done()) */
    const json_t *sup = rp_get(st, "_supervisor");
    bool sup_live = !rp_is_null(sup) && !rp_done(sup);
    if (rp_bool(st, "_reconnect", false) && !sup_live) {
        json_t *a = json_object();
        rp_arg_s(a, "target", "_reconnect_loop");
        rp_arg_s(a, "name", "relay-ws-reconnect");
        json_t *t = rp_call(io, "asyncio.create_task", a);
        if (rp_is_raise(t)) { json_free(r); return t; }
        json_set(st, "_supervisor", t ? t : json_null());
    }
    json_free(r);
    return json_null();
}

/* ── _on_descriptor ── */
/* PoP: rp_on_descriptor @ gateway/relay/ws_transport.py:_on_descriptor */
static json_t *rp_on_descriptor(rp_io_t *io, const json_t *frame, json_t *st)
{
    if (!rp_is_obj(frame)) return rp_attr_raise(frame, "get");
    /* json.dumps(frame.get("descriptor", {})) — absent -> {}, present-null kept. */
    const json_t *empty = json_object();
    const json_t *d = rp_get_def(frame, "descriptor", empty);
    json_t *a = json_object();
    rp_arg_j(a, "obj", d);
    json_t *s = rp_call(io, "json.dumps", a);
    json_free((json_t *)empty);
    if (rp_is_raise(s)) return s;
    a = json_object();
    rp_arg_j(a, "s", s);
    json_t *desc = rp_call(io, "CapabilityDescriptor.from_json", a);
    json_free(s);
    if (rp_is_raise(desc)) return desc;
    if (!rp_is_obj(desc)) { json_t *r = rp_attr_raise(desc, "platform"); json_free(desc); return r; }
    const json_t *platform = rp_get(desc, "platform");
    if (!platform) {   /* absent attr on the SimpleNamespace double */
        json_free(desc);
        return rp_mkraise("AttributeError",
                          "'types.SimpleNamespace' object has no attribute 'platform'");
    }
    if (rp_truth(platform)) {
        char *key = rp_pystr(platform);
        json_t *map = rp_ensure_map(st, "_descriptors_by_platform");
        json_set(map, key, json_copy(desc));
        free(key);
    }
    if (rp_is_null(rp_get(st, "_descriptor"))) json_set(st, "_descriptor", json_copy(desc));
    json_set(st, "_handshake_succeeded", json_bool(true));
    json_t *ready = rp_get(st, "_descriptor_ready");
    if (!rp_is_null(ready) && !rp_done(ready)) {
        json_t *r = rp_future_set_result(io, ready, desc);
        if (rp_is_raise(r)) { json_free(desc); return r; }
    }
    json_free(desc);
    return json_null();
}

/* ── _on_inbound ── */
/* PoP: rp_on_inbound @ gateway/relay/ws_transport.py:_on_inbound */
static json_t *rp_on_inbound(rp_io_t *io, const json_t *frame, json_t *st)
{
    const json_t *inbound = rp_get(st, "_inbound");
    if (rp_is_null(inbound)) return json_null();
    if (!rp_is_obj(frame)) return rp_attr_raise(frame, "get");
    /* event = _event_from_wire(frame.get("event", {})) — OUTSIDE the try. */
    const json_t *empty = json_object();
    const json_t *ev_raw = rp_get_def(frame, "event", empty);
    json_t *a = json_object();
    rp_arg_j(a, "raw", ev_raw);
    json_t *event = rp_call(io, "_event_from_wire", a);
    json_free((json_t *)empty);
    if (rp_is_raise(event)) return event;
    /* try: with relay_callback(self, event): admitting = await self._inbound(event) */
    json_t *admitting = NULL;
    json_t *caught = NULL;
    a = json_object();
    json_t *selfmark = json_object();
    json_set(selfmark, "__transport__", json_bool(true));
    json_set(a, "transport", selfmark);
    rp_arg_j(a, "event", event);
    json_t *rc = rp_call(io, "relay_callback", a);
    if (rp_is_raise(rc)) {
        caught = rc;
    } else {
        json_free(rc);
        json_t *args = json_array();
        json_append(args, json_copy(event));
        admitting = rp_invoke_callable(io, inbound, args, json_object());
        if (rp_is_raise(admitting)) { caught = admitting; admitting = NULL; }
    }
    if (caught) {
        json_free(event);
        if (rp_passthrough(caught)) return caught;
        json_free(caught);
        json_t *lr = rp_log(io, "exception", "relay: inbound handler failed; frame left unacknowledged");
        if (rp_is_raise(lr)) return lr;
        json_free(lr);
        return json_null();
    }
    /* buffer_id = frame.get("bufferId") */
    const json_t *buffer_id = rp_get(frame, "bufferId");
    if (!rp_is_null(admitting)) {
        if (!rp_is_obj(admitting)) { json_t *r = rp_attr_raise(admitting, "add_done_callback"); json_free(admitting); json_free(event); return r; }
        json_t *r = rp_task_add_done_callback(io, admitting, "<lambda>");
        json_free(admitting);
        json_free(event);
        if (rp_is_raise(r)) return r;
        return json_null();
    }
    json_free(admitting);
    if (rp_truth(buffer_id)) {
        char *s = rp_pystr(buffer_id);
        a = json_object();
        rp_arg_s(a, "buffer_id", s);
        free(s);
        json_t *r = rp_call(io, "_send_inbound_ack", a);
        json_free(event);
        if (rp_is_raise(r)) return r;
        json_free(r);
        return json_null();
    }
    json_free(event);
    return json_null();
}

/* ── _ack_admitted ── */
/* PoP: rp_ack_admitted @ gateway/relay/ws_transport.py:_ack_admitted */
static json_t *rp_ack_admitted(rp_io_t *io, const json_t *admission, const json_t *buffer_id, json_t *st)
{
    bool cancelled = rp_bool(admission, "cancelled", false);
    const json_t *exc = rp_get(admission, "exception");
    bool has_exc = !rp_is_null(exc);
    if (cancelled || has_exc) {
        if (!cancelled) {
            json_t *lr = rp_log_arr(io, "error",
                "relay: inbound handler failed; frame left unacknowledged", json_array(), exc);
            if (rp_is_raise(lr)) return lr;
            json_free(lr);
        }
        return json_null();
    }
    if (rp_truth(buffer_id)) {
        char *s = rp_pystr(buffer_id);
        json_t *a = json_object();
        rp_arg_s(a, "target", "_send_inbound_ack");
        /* asyncio.create_task(self._send_inbound_ack(str(buffer_id))) — no name. */
        json_t *ack = rp_call(io, "asyncio.create_task", a);
        free(s);
        if (rp_is_raise(ack)) return ack;
        /* self._inbound_acks.add(ack) — set of ids, kept sorted. */
        json_t *acks = rp_get(st, "_inbound_acks");
        if (!rp_is_arr(acks)) { acks = json_array(); json_set(st, "_inbound_acks", acks); }
        const char *id = rp_str(rp_get(ack, "id"));
        const char *idv = id ? id : "";
        bool present = false;
        size_t pos = acks->c.count;
        for (size_t i = 0; i < acks->c.count; i++) {
            const char *cur = rp_str(acks->c.items[i]);
            if (cur && strcmp(cur, idv) == 0) { present = true; break; }
            if (cur && strcmp(cur, idv) > 0) { pos = i; break; }
        }
        if (!present) {
            /* insert at sorted position */
            json_append(acks, json_string(idv));
            for (size_t i = acks->c.count - 1; i > pos; i--) {
                json_t *tmp = acks->c.items[i];
                acks->c.items[i] = acks->c.items[i - 1];
                acks->c.items[i - 1] = tmp;
            }
        }
        json_t *r = rp_task_add_done_callback(io, ack, "discard");
        json_free(ack);
        if (rp_is_raise(r)) return r;
    }
    return json_null();
}

/* ── _on_going_idle_ack ── */
/* PoP: rp_on_going_idle_ack @ gateway/relay/ws_transport.py:_on_going_idle_ack */
static json_t *rp_on_going_idle_ack(rp_io_t *io, const json_t *frame, json_t *st)
{
    (void)frame;
    json_t *ack = rp_get(st, "_going_idle_ack");
    if (!rp_is_null(ack) && !rp_done(ack)) {
        json_t *r = rp_future_set_result(io, ack, NULL);
        if (rp_is_raise(r)) return r;
    }
    return json_null();
}

/* ── _on_outbound_result ── */
/* PoP: rp_on_outbound_result @ gateway/relay/ws_transport.py:_on_outbound_result */
static json_t *rp_on_outbound_result(rp_io_t *io, const json_t *frame, json_t *st)
{
    if (!rp_is_obj(frame)) return rp_attr_raise(frame, "get");
    const json_t *rid = rp_get_def(frame, "requestId", NULL);
    const json_t *empty_str = json_string("");
    const char *key = rp_is_str(rid) ? rid->str_val : (rid ? NULL : empty_str->str_val);
    json_t *pending = rp_ensure_map(st, "_pending");
    json_t *fut = key ? json_obj_get(pending, key) : NULL;
    json_free((json_t *)empty_str);
    if (!rp_is_null(fut) && !rp_done(fut)) {
        const json_t *empty = json_object();
        const json_t *result = rp_get_def(frame, "result", empty);
        json_t *r = rp_future_set_result(io, fut, result);
        json_free((json_t *)empty);
        if (rp_is_raise(r)) return r;
    }
    return json_null();
}

/* ── _on_interrupt_inbound ── */
/* PoP: rp_on_interrupt_inbound @ gateway/relay/ws_transport.py:_on_interrupt_inbound */
static json_t *rp_on_interrupt_inbound(rp_io_t *io, const json_t *frame, json_t *st)
{
    const json_t *handler = rp_get(st, "_interrupt_inbound_handler");
    if (rp_is_null(handler)) return json_null();
    if (!rp_is_obj(frame)) return rp_attr_raise(frame, "get");
    const json_t *empty = json_string("");
    const json_t *sk = rp_get_def(frame, "session_key", empty);
    const json_t *cid = rp_get_def(frame, "chat_id", empty);
    json_t *args = json_array();
    json_append(args, rp_copy_or_null(sk));
    json_append(args, rp_copy_or_null(cid));
    json_free((json_t *)empty);
    json_t *r = rp_invoke_callable(io, handler, args, json_object());
    if (rp_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* ── _on_passthrough_forward ── */
/* PoP: rp_on_passthrough_forward @ gateway/relay/ws_transport.py:_on_passthrough_forward */
static json_t *rp_on_passthrough_forward(rp_io_t *io, const json_t *frame, json_t *st)
{
    const json_t *handler = rp_get(st, "_passthrough_handler");
    if (rp_is_null(handler)) return json_null();
    if (!rp_is_obj(frame)) return rp_attr_raise(frame, "get");
    const json_t *empty = json_object();
    const json_t *fwd_raw = rp_get_def(frame, "forward", empty);
    json_t *a = json_object();
    rp_arg_j(a, "raw", fwd_raw);
    json_t *fwd = rp_call(io, "_passthrough_from_wire", a);
    json_free((json_t *)empty);
    if (rp_is_raise(fwd)) return fwd;
    /* with relay_callback(self, fwd): await handler(fwd, frame.get("bufferId")) */
    a = json_object();
    json_t *selfmark = json_object();
    json_set(selfmark, "__transport__", json_bool(true));
    json_set(a, "transport", selfmark);
    rp_arg_j(a, "event", fwd);
    json_t *rc = rp_call(io, "relay_callback", a);
    if (rp_is_raise(rc)) { json_free(fwd); return rc; }
    json_free(rc);
    json_t *args = json_array();
    json_append(args, json_copy(fwd));
    json_append(args, rp_copy_or_null(rp_get(frame, "bufferId")));
    json_free(fwd);
    json_t *r = rp_invoke_callable(io, handler, args, json_object());
    if (rp_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* ════════════════════════ dispatch wrappers ════════════════════════ */
static json_t *rp_w_per_url(rp_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *key = rp_str(rp_get(in, "key"));
    return rp_per_url_from_wire(io, rp_get(in, "raw"), key ? key : "");
}
static json_t *rp_w_media_types(rp_io_t *io, const json_t *in, json_t *st)
{ (void)st; return rp_media_types_from_wire(io, rp_get(in, "raw")); }
static json_t *rp_w_media_filenames(rp_io_t *io, const json_t *in, json_t *st)
{ (void)st; return rp_media_filenames_from_wire(io, rp_get(in, "raw")); }
static json_t *rp_w_await_bounded(rp_io_t *io, const json_t *in, json_t *st)
{ (void)st; return rp_await_bounded(io, rp_get(in, "aw")); }
static json_t *rp_w_pending_init(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_pending_request_init(io, rp_get(in, "op"), rp_get(in, "loop"), st); }
static json_t *rp_w_fail_pending(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_fail_pending(io, rp_get(in, "settle"), st); }
static json_t *rp_w_settle_lost(rp_io_t *io, const json_t *in, json_t *st)
{ (void)in; return rp_settle_connection_lost(io, rp_get(st, "fut"), st); }
static json_t *rp_w_close_reason(rp_io_t *io, const json_t *in, json_t *st)
{ (void)st; return rp_close_reason_of(io, rp_get(in, "exc")); }
static json_t *rp_w_latch(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_latch_if_fresh_token_refused(io, rp_get(in, "exc"), st); }
static json_t *rp_w_on_descriptor(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_on_descriptor(io, rp_get(in, "frame"), st); }
static json_t *rp_w_on_inbound(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_on_inbound(io, rp_get(in, "frame"), st); }
static json_t *rp_w_ack_admitted(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_ack_admitted(io, rp_get(in, "admission"), rp_get(in, "buffer_id"), st); }
static json_t *rp_w_on_going_idle_ack(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_on_going_idle_ack(io, rp_get(in, "frame"), st); }
static json_t *rp_w_on_outbound_result(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_on_outbound_result(io, rp_get(in, "frame"), st); }
static json_t *rp_w_on_interrupt(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_on_interrupt_inbound(io, rp_get(in, "frame"), st); }
static json_t *rp_w_on_passthrough(rp_io_t *io, const json_t *in, json_t *st)
{ return rp_on_passthrough_forward(io, rp_get(in, "frame"), st); }

typedef json_t *(*rp_fn_t)(rp_io_t *, const json_t *, json_t *);
typedef struct { const char *func; rp_fn_t fn; } rp_dispatch_t;
static const rp_dispatch_t RP_DISPATCH[] = {
    { "_per_url_from_wire", rp_w_per_url },
    { "_media_types_from_wire", rp_w_media_types },
    { "_media_filenames_from_wire", rp_w_media_filenames },
    { "_await_bounded", rp_w_await_bounded },
    { "_PendingRequest.__init__", rp_w_pending_init },
    { "__init__", rp_wst_init },
    { "_fail_pending", rp_w_fail_pending },
    { "_settle_connection_lost", rp_w_settle_lost },
    { "_dialer_running", rp_dialer_running },
    { "_close_reason_of", rp_w_close_reason },
    { "_latch_if_fresh_token_refused", rp_w_latch },
    { "hold_redial", rp_hold_redial },
    { "release_redial", rp_release_redial },
    { "_await_redial_hold", rp_await_redial_hold },
    { "_redial_with_fresh_token", rp_redial_with_fresh_token },
    { "_on_descriptor", rp_w_on_descriptor },
    { "_on_inbound", rp_w_on_inbound },
    { "_ack_admitted", rp_w_ack_admitted },
    { "_on_going_idle_ack", rp_w_on_going_idle_ack },
    { "_on_outbound_result", rp_w_on_outbound_result },
    { "_on_interrupt_inbound", rp_w_on_interrupt },
    { "_on_passthrough_forward", rp_w_on_passthrough },
};
#define RP_DISPATCH_N (sizeof(RP_DISPATCH) / sizeof(RP_DISPATCH[0]))
