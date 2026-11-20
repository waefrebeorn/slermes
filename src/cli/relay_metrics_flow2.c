/* relay_metrics_flow2.c — FLOW port of the UNCLAIMED fourth-wave remainder of
 * hermes_cli/observability/relay_shared_metrics.py (the relay-metrics module;
 * the task brief named "relay_metrics.py", which does not exist in the quarry —
 * per the skill's defining-module rule every PoP below names the REAL defining
 * module). The claimed siblings live in relay_shared_metrics.c / _flow.c /
 * _gaps, src/cli/relay_metrics_flow.c and src/cli/relay_shared_metrics_flow2.c
 * and are SEAMED here through the injected universal seam rm2_io_t.
 *
 * Decision-sequence port (10 functions): the connector-batch settle
 * (_settle_connector_batch), the turn-observation pair (_observe_model_turn /
 * _observe_turn_call), the cold-cache break recorder (record_known_cache_break
 * method), the toolset-snapshot recorder (record_session_tools method), the
 * compression hand-off (rotate_segment method), the session-close summary
 * (_emit_session_summary), the process-mark emitters (record_process_mark /
 * record_process_marks_saved methods) and the send-pass consent closure
 * (still_consented, nested in _run_send_pass).
 * ALL collaborator I/O (claimed siblings, foreign-module imports, the Relay
 * host/subscriber/store/clock, logger, uuid, the connectors/engagement/model/
 * efficiency helpers) rides the universal seam; the harness scripts results and
 * records the ordered trace the oracle diffs against LIVE relay_shared_metrics.py
 * under the same scripts. None of the ten ported bodies carries a try/except
 * guard (every guard is a seamed _guarded), so a scripted BaseException passes
 * straight through to the case envelope exactly as live; the FULL builtin
 * exception-parent table is carried for raise-spec classification parity with
 * the sibling ports.
 *
 * Representation contract (shared with tests/oracle_relay_metrics_flow2.py):
 *   - raise specs are VALUES {"__raise__": kind, "message": msg} threaded through
 *     every helper; an uncaught one becomes the case ret.
 *   - fail-closed: an absent / exhausted seam script returns a RuntimeError raise
 *     spec ("no script for <name>"), recorded as the event ret BEFORE propagation.
 *   - object doubles (session/task/model_call/lineage/tools/efficiency/cost/
 *     model_state/resolved/subscriber) are attribute dicts read via rm2_get;
 *     tuple/set -> array (sets sorted); None -> null; integral floats collapse
 *     to ints on both sides.
 *   - session.tool_calls (a tuple-keyed dict live) travels as an INSERTION-ORDERED
 *     array of [key4, call_record] pairs; key4 = [task_id, api_request_id, turn_id,
 *     tool_call_id]. call_record is a plain attribute dict (no method reads on it
 *     in this port's scope). task.completed_tool_call_ids / tool_call_ids (sets of
 *     3-tuples) travel as arrays of 3-arrays sorted by the Python str(list) key
 *     "['a', 'b', 'c']" (ASCII identifier elements only); model_call_ids / turn_ids
 *     and lineage.open (sets of str) travel as strcmp-sorted string arrays.
 *   - a rt._session seam result may be the marker {"__session_ref__": "<state key>"}
 *     resolving to the state carrier double (so mutations echo in out.state), a
 *     plain record (fresh double, mutations not echoed), or null (None).
 *   - the lambda `run: run.observe(task.selected_route)` passed to the seamed
 *     _with_route_run projects to {"__callable__": "run.observe"}; the scripted
 *     _with_route_run double never invokes it (its body is out of seam scope, the
 *     same convention as the sibling still_consented projection).
 *   - scripts: a bare JSON array is a per-call QUEUE (one result per call); a
 *     list RESULT must be wrapped {"__script__": true, "ret": [...]}. A scripted
 *     null returns None. None-valued seam-arg OBJECT keys dropped on both sides.
 *   - fixture domain (ASCII scope): object seam results are null or a record;
 *     attribute doubles carry the fields the live body reads; fields/event are
 *     dicts; tools_for_api is a list or null; eff.tool_snapshot results are lists
 *     (the *splat domain); rt._close_segment results are 3-arrays; marks are lists
 *     of [mark, data] pairs; uuid.uuid4 results are null or {"hex": ...};
 *     _resolved_send_config results are null or a {send, endpoint} record;
 *     _event_metadata results are dicts; id() results are scalars. Lineage records
 *     carry open/peak/tally/tools/run/spent/connectors (mk_lineage key order);
 *     a NEWLY created _PeakLineage projects in dataclass field order
 *     open/peak/tools/tally/run/spent/connectors. EXCLUDED: integral-FLOAT literals
 *     in str() positions (libjson has no int/float identity); non-list tool_snapshot
 *     results and non-3-array _close_segment results (stub-qualname binding
 *     TypeErrors are oracle-injection-dependent); non-ASCII identity elements.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/relay_metrics_flow2.c". rm_ / rsm_ / rsm2_ prefixes are taken by
 * the sibling ports; this port uses rm2_.
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
typedef struct rm2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} rm2_io_t;

/* ── truthiness / accessors (Python-faithful; no libjson string coercion) ── */
static bool rm2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool rm2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rm2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool rm2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool rm2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool rm2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool rm2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}

static const char *rm2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *rm2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static bool rm2_str_eq(const char *a, const char *b)
{
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* ── raise specs ── */
static bool rm2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *rm2_kind(const json_t *r)
{
    const char *k = rm2_str(rm2_get(r, "__raise__"));
    return k ? k : "";
}
static json_t *rm2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *rm2_xfmt(const char *fmt, ...)
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
 * default to the Exception family. Carried for raise-spec classification parity
 * with the sibling ports (this port's ten bodies have no except guard). ── */
typedef struct rm2_exc_parent { const char *kind; const char *parent; } rm2_exc_parent_t;
static const rm2_exc_parent_t RM2_EXC_PARENTS[] = {
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
static bool rm2_exc_is_exception(const char *kind)
{
    char cur[128];
    snprintf(cur, sizeof cur, "%s", kind ? kind : "");
    for (int hops = 0; hops < 16; hops++) {
        if (rm2_str_eq(cur, "Exception")) return true;
        if (rm2_str_eq(cur, "BaseException") || rm2_str_eq(cur, "")) return false;
        const char *parent = NULL;
        for (size_t i = 0; i < sizeof(RM2_EXC_PARENTS) / sizeof(RM2_EXC_PARENTS[0]); i++) {
            if (rm2_str_eq(RM2_EXC_PARENTS[i].kind, cur)) { parent = RM2_EXC_PARENTS[i].parent; break; }
        }
        if (!parent) return true;   /* unknown synthetic kind: Exception subclass */
        snprintf(cur, sizeof cur, "%s", parent);
    }
    return false;
}
static bool rm2_is_base(const char *kind) { return !rm2_exc_is_exception(kind); }

/* ── universal seam call: args OWNED by caller, freed here; result caller-owned
 * (NULL == None, or a raise spec). Fail-closed when no channel installed. ── */
static json_t *rm2_call(rm2_io_t *io, const char *name, json_t *args)
{
    if (!io || !io->call) {
        char *m = rm2_xfmt("no seam installed for %s", name);
        json_t *r = rm2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* arg builders: None-valued keys dropped on both sides ("" kept). */
static void rm2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void rm2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void rm2_arg_b(json_t *a, const char *k, bool v)
{
    json_set(a, k, json_bool(v));
}
static void rm2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* attribute read on a double: present value (copy) or the live AttributeError.
 * null receiver -> NoneType message; absent field -> SimpleNamespace message. */
static json_t *rm2_read_attr(const json_t *obj, const char *key)
{
    if (rm2_is_null(obj)) {
        char *m = rm2_xfmt("'NoneType' object has no attribute '%s'", key);
        json_t *r = rm2_mkraise("AttributeError", m ? m : "");
        free(m);
        return r;
    }
    const json_t *v = rm2_get(obj, key);
    if (!v) {
        char *m = rm2_xfmt("'types.SimpleNamespace' object has no attribute '%s'", key);
        json_t *r = rm2_mkraise("AttributeError", m ? m : "");
        free(m);
        return r;
    }
    return json_copy(v);
}

/* receiver attribute check before a seamed method call: NULL when the receiver
 * is present and non-null (borrowed into *recv_out), else the live AttributeError
 * (absent receiver -> SimpleNamespace; present-null -> NoneType naming METHOD). */
static json_t *rm2_method_recv(const json_t *obj, const char *recv_key,
                               const char *method, const json_t **recv_out)
{
    json_t *r = rm2_read_attr(obj, recv_key);
    if (rm2_is_raise(r)) return r;
    if (rm2_is_null(r)) {
        json_free(r);
        char *m = rm2_xfmt("'NoneType' object has no attribute '%s'", method);
        json_t *e = rm2_mkraise("AttributeError", m ? m : "");
        free(m);
        return e;
    }
    json_free(r);
    if (recv_out) *recv_out = rm2_get(obj, recv_key);
    return NULL;
}

/* deep JSON equality (Python == on the scalar/array/record domain). */
static bool rm2_json_eq(const json_t *a, const json_t *b)
{
    bool an = rm2_is_null(a), bn = rm2_is_null(b);
    if (an || bn) return an && bn;
    if (a->type != b->type) return false;
    switch (a->type) {
    case JSON_BOOL: return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return rm2_str_eq(a->str_val, b->str_val);
    case JSON_ARRAY:
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!rm2_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    case JSON_OBJECT:
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !rm2_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    default: return false;
    }
}

/* sorted-string-set add (lineage.open / model_call_ids / turn_ids): strcmp order,
 * dedupe. arr is the state carrier array (mutated in place). */
static void rm2_set_add_str(json_t *arr, const char *s)
{
    if (!rm2_is_arr(arr) || !s) return;
    size_t i = 0;
    while (i < arr->c.count) {
        const char *cur = rm2_str(arr->c.items[i]);
        int c = strcmp(cur ? cur : "", s);
        if (c == 0) return;            /* already present */
        if (c > 0) break;              /* insert before i */
        i++;
    }
    json_t *node = json_string(s);
    /* shift-insert at i */
    json_append(arr, json_null());     /* grow by one slot */
    for (size_t j = arr->c.count - 1; j > i; j--) {
        json_free(arr->c.items[j]);
        arr->c.items[j] = arr->c.items[j - 1];
    }
    arr->c.items[i] = node;
}

/* Python str(list) sort key for a 3-array of ASCII identifier strings:
 * "['a', 'b', 'c']". Caller frees. */
static char *rm2_identity_key(const json_t *id3)
{
    const char *a = rm2_str(json_get(id3, 0));
    const char *b = rm2_str(json_get(id3, 1));
    const char *c = rm2_str(json_get(id3, 2));
    return rm2_xfmt("['%s', '%s', '%s']", a ? a : "", b ? b : "", c ? c : "");
}

/* sorted-identity-set add (completed_tool_call_ids / tool_call_ids): order by the
 * Python str(list) key, dedupe by deep equality. arr mutated in place. */
static void rm2_set_add_identity(json_t *arr, const json_t *id3)
{
    if (!rm2_is_arr(arr) || !rm2_is_arr(id3)) return;
    char *key = rm2_identity_key(id3);
    size_t i = 0;
    while (i < arr->c.count) {
        if (rm2_json_eq(arr->c.items[i], id3)) { free(key); return; }
        char *ck = rm2_identity_key(arr->c.items[i]);
        int c = strcmp(ck ? ck : "", key ? key : "");
        free(ck);
        if (c > 0) break;
        i++;
    }
    json_append(arr, json_null());
    for (size_t j = arr->c.count - 1; j > i; j--) {
        json_free(arr->c.items[j]);
        arr->c.items[j] = arr->c.items[j - 1];
    }
    arr->c.items[i] = json_copy(id3);
    free(key);
}

/* resolve a rt._session seam result: {"__session_ref__": key} -> the state carrier
 * (borrowed); a plain record -> itself; null/NULL -> NULL (None). */
static const json_t *rm2_session_ref(const json_t *raw, const json_t *st)
{
    if (rm2_is_obj(raw)) {
        const char *ref = rm2_str(rm2_get(raw, "__session_ref__"));
        if (ref) return rm2_get(st, ref);
    }
    return rm2_is_null(raw) ? NULL : raw;
}

/* a callable double marker {"__callable__": name}. */
static json_t *rm2_callable(const char *name)
{
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string(name));
    return cb;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 1. _settle_connector_batch — a connector batch wrapper's post settles its
 * still-open entries with the wrapper's outcome; True when settled this way.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_settle_connector_batch @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._settle_connector_batch */
static json_t *rm2_settle_connector_batch(rm2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *session = rm2_get(st, "session");
    const json_t *task = rm2_get(st, "task");
    const json_t *wrapper = rm2_get(in, "wrapper");
    const json_t *event = rm2_get(in, "event");
    /* if not connectors_.is_connector_batch(event): return False */
    json_t *ba = json_object();
    rm2_arg_j(ba, "event", event);
    json_t *isbatch = rm2_call(io, "connectors.is_connector_batch", ba);
    if (rm2_is_raise(isbatch)) return isbatch;
    bool batch = rm2_truth(isbatch);
    json_free(isbatch);
    if (!batch) return json_bool(false);
    /* started = [key for key in session.tool_calls
     *            if key[0] == task.task_id and connectors_.is_batch_entry(key[1:], wrapper)] */
    json_t *tc_probe = rm2_read_attr(session, "tool_calls");
    if (rm2_is_raise(tc_probe)) return tc_probe;
    if (rm2_is_null(tc_probe)) {
        json_free(tc_probe);
        return rm2_mkraise("TypeError", "'NoneType' object is not iterable");
    }
    json_free(tc_probe);
    json_t *tool_calls = rm2_get(session, "tool_calls");   /* BORROWED carrier (pop mutates it) */
    json_t *task_id = rm2_read_attr(task, "task_id");
    if (rm2_is_raise(task_id)) return task_id;
    const char *tid = rm2_str(task_id);
    json_t *started = json_array();   /* owned key4 copies */
    if (rm2_is_arr(tool_calls)) {
        for (size_t i = 0; i < tool_calls->c.count; i++) {
            const json_t *entry = tool_calls->c.items[i];
            const json_t *key = rm2_is_arr(entry) ? json_get(entry, 0) : NULL;
            if (!rm2_is_arr(key) || json_len(key) < 4) continue;
            if (!rm2_str_eq(rm2_str(json_get(key, 0)), tid)) continue;
            json_t *id3 = json_array();
            for (size_t j = 1; j < 4; j++) json_append(id3, json_copy(json_get(key, j)));
            json_t *ea = json_object();
            json_set(ea, "identity", id3);
            rm2_arg_j(ea, "wrapper", wrapper);
            json_t *isentry = rm2_call(io, "connectors.is_batch_entry", ea);
            if (rm2_is_raise(isentry)) {
                json_free(started); json_free(task_id);
                return isentry;
            }
            bool is_entry = rm2_truth(isentry);
            json_free(isentry);
            if (is_entry) json_append(started, json_copy(key));
        }
    }
    json_free(task_id);
    /* if not started and not connectors_.batch_entry_ran(task.completed_tool_call_ids, wrapper):
     *     return False */
    if (json_len(started) == 0) {
        json_t *completed = rm2_read_attr(task, "completed_tool_call_ids");
        if (rm2_is_raise(completed)) { json_free(started); return completed; }
        json_t *ra = json_object();
        rm2_arg_j(ra, "identities", completed);
        json_free(completed);
        rm2_arg_j(ra, "wrapper", wrapper);
        json_t *ran = rm2_call(io, "connectors.batch_entry_ran", ra);
        if (rm2_is_raise(ran)) { json_free(started); return ran; }
        bool did = rm2_truth(ran);
        json_free(ran);
        if (!did) {
            json_free(started);
            return json_bool(false);
        }
    }
    /* for key in started:
     *     task.completed_tool_call_ids.add(key[1:])
     *     self._finish_tool_call(task, session.tool_calls.pop(key), event) */
    json_t *completed = rm2_read_attr(task, "completed_tool_call_ids");
    if (rm2_is_raise(completed)) { json_free(started); return completed; }
    json_free(completed);   /* presence check; the carrier array is borrowed below */
    json_t *comp_arr = rm2_get(task, "completed_tool_call_ids");
    for (size_t i = 0; i < started->c.count; i++) {
        const json_t *key = started->c.items[i];
        json_t *id3 = json_array();
        for (size_t j = 1; j < 4; j++) json_append(id3, json_copy(json_get(key, j)));
        rm2_set_add_identity(comp_arr, id3);
        json_free(id3);
        /* pop the entry from session.tool_calls (snapshot value BEFORE removal) */
        json_t *call_val = json_null();
        if (rm2_is_arr(tool_calls)) {
            for (size_t k = 0; k < tool_calls->c.count; k++) {
                const json_t *entry = tool_calls->c.items[k];
                const json_t *ek = rm2_is_arr(entry) ? json_get(entry, 0) : NULL;
                if (rm2_json_eq(ek, key)) {
                    const json_t *v = json_get(entry, 1);
                    json_free(call_val);
                    call_val = v ? json_copy(v) : json_null();
                    json_array_remove(tool_calls, k);
                    break;
                }
            }
        }
        json_t *fa = json_object();
        rm2_arg_j(fa, "task", task);
        json_set(fa, "tool_call", call_val);   /* ownership transfer */
        rm2_arg_j(fa, "event", event);
        json_t *r = rm2_call(io, "rt._finish_tool_call", fa);
        if (rm2_is_raise(r)) { json_free(started); return r; }
        json_free(r);
    }
    /* task.completed_tool_call_ids.add(wrapper); return True */
    rm2_set_add_identity(comp_arr, wrapper);
    json_free(started);
    return json_bool(true);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. _observe_model_turn — trailing-failure state, engagement / friction marks
 * and the efficiency turn rows for a turn the user saw end.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_observe_model_turn @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._observe_model_turn */
static json_t *rm2_observe_model_turn(rm2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *session = rm2_get(st, "session");
    const json_t *task = rm2_get(st, "task");
    const json_t *fields = rm2_get(in, "fields");
    /* route = task.model_route or session.model_state.last_route */
    json_t *model_route = rm2_read_attr(task, "model_route");
    if (rm2_is_raise(model_route)) return model_route;
    json_t *route = NULL;   /* owned */
    if (rm2_truth(model_route)) {
        route = model_route;   /* ownership transfer */
    } else {
        json_free(model_route);
        const json_t *ms = NULL;
        json_t *bad = rm2_method_recv(session, "model_state", "last_route", &ms);
        if (bad) return bad;
        route = rm2_read_attr(ms, "last_route");
        if (rm2_is_raise(route)) return route;
    }
    /* session.model_state.observe_turn(fields["outcome"], route, session.last_turn_ns) */
    const json_t *ms2 = NULL;
    json_t *bad = rm2_method_recv(session, "model_state", "observe_turn", &ms2);
    if (bad) { json_free(route); return bad; }
    const json_t *outcome = rm2_get(fields, "outcome");
    if (!outcome) {
        json_free(route);
        return rm2_mkraise("KeyError", "'outcome'");
    }
    json_t *last_ns = rm2_read_attr(session, "last_turn_ns");
    if (rm2_is_raise(last_ns)) { json_free(route); return last_ns; }
    json_t *oa = json_object();
    rm2_arg_j(oa, "outcome", outcome);
    rm2_arg_j(oa, "route", route);
    rm2_arg_j(oa, "last_turn_ns", last_ns);
    json_free(last_ns);
    json_t *r = rm2_call(io, "model_state.observe_turn", oa);
    if (rm2_is_raise(r)) { json_free(route); return r; }
    json_free(r);
    /* if task.selected_route is not None and task.cost.user_turn:
     *     self._with_route_run(session.session_id, session.route_run, lambda run: run.observe(...)) */
    json_t *selected = rm2_read_attr(task, "selected_route");
    if (rm2_is_raise(selected)) { json_free(route); return selected; }
    bool has_selected = !rm2_is_null(selected);
    json_free(selected);
    if (has_selected) {
        const json_t *cost = NULL;
        bad = rm2_method_recv(task, "cost", "user_turn", &cost);
        if (bad) { json_free(route); return bad; }
        json_t *ut = rm2_read_attr(cost, "user_turn");
        if (rm2_is_raise(ut)) { json_free(route); return ut; }
        bool user_turn = rm2_truth(ut);
        json_free(ut);
        if (user_turn) {
            json_t *sid = rm2_read_attr(session, "session_id");
            if (rm2_is_raise(sid)) { json_free(route); return sid; }
            json_t *route_run = rm2_read_attr(session, "route_run");
            if (rm2_is_raise(route_run)) { json_free(sid); json_free(route); return route_run; }
            json_t *cb = rm2_callable("run.observe");
            json_t *wa = json_object();
            rm2_arg_j(wa, "session_id", sid);
            json_free(sid);
            rm2_arg_j(wa, "own", route_run);
            json_free(route_run);
            json_set(wa, "update", cb);   /* ownership transfer */
            r = rm2_call(io, "rt._with_route_run", wa);
            if (rm2_is_raise(r)) { json_free(route); return r; }
            json_free(r);
        }
    }
    /* if task.model_route is not None and engagement_.engaged_turn(task.start_fields, task.cost.user_turn):
     *     self._guarded("...engagement mark failed", self._mark, session, None, ENGAGEMENT_TURN_MARK, dict(task.model_route)) */
    json_t *mr = rm2_read_attr(task, "model_route");
    if (rm2_is_raise(mr)) { json_free(route); return mr; }
    bool has_mr = !rm2_is_null(mr);
    if (has_mr) {
        json_t *start_fields = rm2_read_attr(task, "start_fields");
        if (rm2_is_raise(start_fields)) { json_free(mr); json_free(route); return start_fields; }
        const json_t *cost = NULL;
        bad = rm2_method_recv(task, "cost", "user_turn", &cost);
        if (bad) { json_free(start_fields); json_free(mr); json_free(route); return bad; }
        json_t *ut = rm2_read_attr(cost, "user_turn");
        if (rm2_is_raise(ut)) { json_free(start_fields); json_free(mr); json_free(route); return ut; }
        json_t *ea = json_object();
        rm2_arg_j(ea, "start_fields", start_fields);
        json_free(start_fields);
        rm2_arg_b(ea, "user_turn", rm2_truth(ut));
        json_free(ut);
        json_t *engaged = rm2_call(io, "engagement.engaged_turn", ea);
        if (rm2_is_raise(engaged)) { json_free(mr); json_free(route); return engaged; }
        bool eng = rm2_truth(engaged);
        json_free(engaged);
        if (eng) {
            json_t *cb = rm2_callable("rt._mark");
            json_t *ga = json_object();
            rm2_arg_s(ga, "message", "Hermes shared-metrics engagement mark failed");
            json_set(ga, "callback", cb);
            json_t *gargs = json_array();
            json_append(gargs, session ? json_copy(session) : json_null());   /* array element: null kept */
            json_append(gargs, json_null());
            json_append(gargs, json_string("hermes.engagement.turn"));
            json_append(gargs, json_copy(mr));      /* dict(task.model_route) */
            json_set(ga, "args", gargs);
            json_set(ga, "kwargs", json_object());
            r = rm2_call(io, "rt._guarded", ga);
            if (rm2_is_raise(r)) { json_free(mr); json_free(route); return r; }
            json_free(r);
        }
    }
    json_free(mr);
    /* if fields["end_reason"] == "user_cancelled" and route is not None and model_.attended(task.start_fields):
     *     self._guarded("...friction mark failed", self._mark, session, None, MODEL_FRICTION_MARK, model_.friction_fields("interrupt", route)) */
    const json_t *end_reason = rm2_get(fields, "end_reason");
    if (!end_reason) { json_free(route); return rm2_mkraise("KeyError", "'end_reason'"); }
    if (rm2_str_eq(rm2_str(end_reason), "user_cancelled") && !rm2_is_null(route)) {
        json_t *start_fields = rm2_read_attr(task, "start_fields");
        if (rm2_is_raise(start_fields)) { json_free(route); return start_fields; }
        json_t *aa = json_object();
        rm2_arg_j(aa, "start_fields", start_fields);
        json_free(start_fields);
        json_t *attended = rm2_call(io, "model.attended", aa);
        if (rm2_is_raise(attended)) { json_free(route); return attended; }
        bool att = rm2_truth(attended);
        json_free(attended);
        if (att) {
            json_t *fa = json_object();
            rm2_arg_s(fa, "signal", "interrupt");
            rm2_arg_j(fa, "route", route);
            json_t *friction = rm2_call(io, "model.friction_fields", fa);
            if (rm2_is_raise(friction)) { json_free(route); return friction; }
            json_t *cb = rm2_callable("rt._mark");
            json_t *ga = json_object();
            rm2_arg_s(ga, "message", "Hermes shared-metrics friction mark failed");
            json_set(ga, "callback", cb);
            json_t *gargs = json_array();
            json_append(gargs, session ? json_copy(session) : json_null());
            json_append(gargs, json_null());
            json_append(gargs, json_string("hermes.model_friction"));
            json_append(gargs, friction ? friction : json_null());   /* ownership transfer */
            json_set(ga, "args", gargs);
            json_set(ga, "kwargs", json_object());
            r = rm2_call(io, "rt._guarded", ga);
            if (rm2_is_raise(r)) { json_free(route); return r; }
            json_free(r);
        }
    }
    /* if task.cost.user_turn and model_.attended(task.start_fields):
     *     rows = session.efficiency.finish_turn(task.cost, fields, route=route, model_calls=..., tool_calls=...)
     *     self._emit_rows(session, rows) */
    {
        const json_t *cost = NULL;
        bad = rm2_method_recv(task, "cost", "user_turn", &cost);
        if (bad) { json_free(route); return bad; }
        json_t *ut = rm2_read_attr(cost, "user_turn");
        if (rm2_is_raise(ut)) { json_free(route); return ut; }
        bool user_turn = rm2_truth(ut);
        json_free(ut);
        if (user_turn) {
            json_t *start_fields = rm2_read_attr(task, "start_fields");
            if (rm2_is_raise(start_fields)) { json_free(route); return start_fields; }
            json_t *aa = json_object();
            rm2_arg_j(aa, "start_fields", start_fields);
            json_free(start_fields);
            json_t *attended = rm2_call(io, "model.attended", aa);
            if (rm2_is_raise(attended)) { json_free(route); return attended; }
            bool att = rm2_truth(attended);
            json_free(attended);
            if (att) {
                const json_t *eff = NULL;
                bad = rm2_method_recv(session, "efficiency", "finish_turn", &eff);
                if (bad) { json_free(route); return bad; }
                json_t *mcids = rm2_read_attr(task, "model_call_ids");
                if (rm2_is_raise(mcids)) { json_free(route); return mcids; }
                json_t *tcids = rm2_read_attr(task, "tool_call_ids");
                if (rm2_is_raise(tcids)) { json_free(mcids); json_free(route); return tcids; }
                json_t *unid = rm2_read_attr(task, "unidentified_tool_calls");
                if (rm2_is_raise(unid)) { json_free(tcids); json_free(mcids); json_free(route); return unid; }
                double nmc = (double)json_len(mcids);
                double ntc = (double)json_len(tcids) + (rm2_is_num(unid) ? unid->num_val : 0.0);
                json_free(mcids); json_free(tcids); json_free(unid);
                json_t *cost_copy = rm2_read_attr(task, "cost");
                if (rm2_is_raise(cost_copy)) { json_free(route); return cost_copy; }
                json_t *ffa = json_object();
                rm2_arg_j(ffa, "cost", cost_copy);
                json_free(cost_copy);
                rm2_arg_j(ffa, "fields", fields);
                rm2_arg_j(ffa, "route", route);
                rm2_arg_n(ffa, "model_calls", nmc);
                rm2_arg_n(ffa, "tool_calls", ntc);
                json_t *rows = rm2_call(io, "efficiency.finish_turn", ffa);
                if (rm2_is_raise(rows)) { json_free(route); return rows; }
                json_t *era = json_object();
                rm2_arg_j(era, "session", session);
                json_set(era, "rows", rows ? rows : json_null());   /* ownership transfer */
                r = rm2_call(io, "rt._emit_rows", era);
                if (rm2_is_raise(r)) { json_free(route); return r; }
                json_free(r);
            }
        }
    }
    json_free(route);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. _observe_turn_call — one finished primary call of a user turn: its tokens
 * and a cold cache read nobody announced.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_observe_turn_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._observe_turn_call */
static json_t *rm2_observe_turn_call(rm2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *session = rm2_get(st, "session");
    const json_t *task = rm2_get(st, "task");
    const json_t *model_call = rm2_get(st, "model_call");
    const json_t *event = rm2_get(in, "event");
    /* if task is None or not task.cost.user_turn: return */
    if (rm2_is_null(task)) return NULL;
    const json_t *cost = NULL;
    json_t *bad = rm2_method_recv(task, "cost", "user_turn", &cost);
    if (bad) return bad;
    json_t *ut = rm2_read_attr(cost, "user_turn");
    if (rm2_is_raise(ut)) return ut;
    bool user_turn = rm2_truth(ut);
    json_free(ut);
    if (!user_turn) return NULL;
    /* route, usage = model_call.fields, event.get("usage") */
    json_t *route = rm2_read_attr(model_call, "fields");
    if (rm2_is_raise(route)) return route;
    if (rm2_is_null(event)) {
        json_free(route);
        return rm2_mkraise("AttributeError", "'NoneType' object has no attribute 'get'");
    }
    const json_t *usage = rm2_get(event, "usage");
    /* task.cost.add_call(route, usage) */
    json_t *ca = json_object();
    rm2_arg_j(ca, "route", route);
    rm2_arg_j(ca, "usage", usage);
    json_t *r = rm2_call(io, "cost.add_call", ca);
    if (rm2_is_raise(r)) { json_free(route); return r; }
    json_free(r);
    /* expected = self._consume_cold(self._cold_key(session.session_id)) */
    json_t *sid = rm2_read_attr(session, "session_id");
    if (rm2_is_raise(sid)) { json_free(route); return sid; }
    json_t *ka = json_object();
    rm2_arg_j(ka, "session_id", sid);
    json_free(sid);
    json_t *key = rm2_call(io, "rt._cold_key", ka);
    if (rm2_is_raise(key)) { json_free(route); return key; }
    json_t *cka = json_object();
    rm2_arg_j(cka, "key", key);
    json_free(key);
    json_t *expected = rm2_call(io, "rt._consume_cold", cka);
    if (rm2_is_raise(expected)) { json_free(route); return expected; }
    /* cause = session.efficiency.observe_cache(route, usage, model_call.started_ns, monotonic_ns(), expected=expected) */
    json_t *started_ns = rm2_read_attr(model_call, "started_ns");
    if (rm2_is_raise(started_ns)) { json_free(expected); json_free(route); return started_ns; }
    json_t *now = rm2_call(io, "monotonic_ns", json_object());
    if (rm2_is_raise(now)) { json_free(started_ns); json_free(expected); json_free(route); return now; }
    const json_t *eff = NULL;
    bad = rm2_method_recv(session, "efficiency", "observe_cache", &eff);
    if (bad) { json_free(now); json_free(started_ns); json_free(expected); json_free(route); return bad; }
    json_t *oc = json_object();
    rm2_arg_j(oc, "route", route);
    rm2_arg_j(oc, "usage", usage);
    rm2_arg_j(oc, "started_ns", started_ns);
    json_free(started_ns);
    rm2_arg_j(oc, "now_ns", now);
    json_free(now);
    rm2_arg_j(oc, "expected", expected);
    json_free(expected);
    json_t *cause = rm2_call(io, "efficiency.observe_cache", oc);
    if (rm2_is_raise(cause)) { json_free(route); return cause; }
    /* if cause is not None: self._emit_rows(session, [eff.cache_break_row(cause, route)]) */
    if (!rm2_is_null(cause)) {
        json_t *cba = json_object();
        rm2_arg_j(cba, "cause", cause);
        rm2_arg_j(cba, "route", route);
        json_t *row = rm2_call(io, "eff.cache_break_row", cba);
        if (rm2_is_raise(row)) { json_free(cause); json_free(route); return row; }
        json_t *rows = json_array();
        json_append(rows, row ? row : json_null());   /* ownership transfer */
        json_t *era = json_object();
        rm2_arg_j(era, "session", session);
        json_set(era, "rows", rows);
        r = rm2_call(io, "rt._emit_rows", era);
        if (rm2_is_raise(r)) { json_free(cause); json_free(route); return r; }
        json_free(r);
    }
    json_free(cause);
    json_free(route);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. record_known_cache_break (method) — announce a cold read; emit one break
 * row only when the announcement was fresh.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_record_known_cache_break @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_known_cache_break */
static json_t *rm2_record_known_cache_break(rm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *cause = json_get_str(in, "cause", "");
    const json_t *route = rm2_get(in, "route");
    const char *session_id = json_get_str(in, "session_id", "");
    /* if self._announce_cold(self._cold_key(session_id)): */
    json_t *ka = json_object();
    rm2_arg_s(ka, "session_id", session_id);
    json_t *key = rm2_call(io, "rt._cold_key", ka);
    if (rm2_is_raise(key)) return key;
    json_t *aa = json_object();
    rm2_arg_j(aa, "key", key);
    json_free(key);
    json_t *announced = rm2_call(io, "rt._announce_cold", aa);
    if (rm2_is_raise(announced)) return announced;
    bool fresh = rm2_truth(announced);
    json_free(announced);
    if (!fresh) return NULL;
    /* self._emit_rows(None, [eff.cache_break_row(cause, route)]) */
    json_t *cba = json_object();
    rm2_arg_s(cba, "cause", cause);
    rm2_arg_j(cba, "route", route);
    json_t *row = rm2_call(io, "eff.cache_break_row", cba);
    if (rm2_is_raise(row)) return row;
    json_t *rows = json_array();
    json_append(rows, row ? row : json_null());
    json_t *era = json_object();   /* session None -> key dropped */
    json_set(era, "rows", rows);
    json_t *r = rm2_call(io, "rt._emit_rows", era);
    if (rm2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. record_session_tools (method) — the tools one primary request sent; a
 * changed toolset announces a cold cache read.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_record_session_tools @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_session_tools */
static json_t *rm2_record_session_tools(rm2_io_t *io, const json_t *in, json_t *st)
{
    const char *session_id = json_get_str(in, "session_id", "");
    const json_t *agent = rm2_get(in, "agent");
    const json_t *tools_for_api = rm2_get(in, "tools_for_api");
    /* session = self._session({"session_id": session_id}) if session_id else None */
    json_t *sess_raw = NULL;
    const json_t *session = NULL;
    if (session_id[0]) {
        json_t *event = json_object();
        json_set(event, "session_id", json_string(session_id));
        json_t *sa = json_object();
        json_set(sa, "event", event);
        sess_raw = rm2_call(io, "rt._session", sa);
        if (rm2_is_raise(sess_raw)) return sess_raw;
        session = rm2_session_ref(sess_raw, st);
    }
    if (rm2_is_null(session)) { json_free(sess_raw); return NULL; }
    /* key = (id(getattr(agent, "tools", None)), id(tools_for_api), len(tools_for_api)) */
    const json_t *agent_tools = rm2_get(agent, "tools");   /* getattr default None */
    json_t *ia1 = json_object();
    rm2_arg_j(ia1, "value", agent_tools);
    json_t *id1 = rm2_call(io, "id", ia1);
    if (rm2_is_raise(id1)) { json_free(sess_raw); return id1; }
    json_t *ia2 = json_object();
    rm2_arg_j(ia2, "value", tools_for_api);
    json_t *id2 = rm2_call(io, "id", ia2);
    if (rm2_is_raise(id2)) { json_free(id1); json_free(sess_raw); return id2; }
    if (!rm2_is_arr(tools_for_api)) {
        json_free(id1); json_free(id2); json_free(sess_raw);
        return rm2_mkraise("TypeError", "object of type 'NoneType' has no len()");
    }
    json_t *key = json_array();
    json_append(key, id1 ? id1 : json_null());   /* ownership transfer */
    json_append(key, id2 ? id2 : json_null());
    json_append(key, json_number((double)json_len(tools_for_api)));
    /* with session.lock: if session.closing or session.efficiency.tools_key == key: return */
    json_t *closing = rm2_read_attr(session, "closing");
    if (rm2_is_raise(closing)) { json_free(key); json_free(sess_raw); return closing; }
    bool is_closing = rm2_truth(closing);
    json_free(closing);
    if (is_closing) { json_free(key); json_free(sess_raw); return NULL; }
    const json_t *eff = NULL;
    json_t *bad = rm2_method_recv(session, "efficiency", "tools_key", &eff);
    if (bad) { json_free(key); json_free(sess_raw); return bad; }
    json_t *tools_key = rm2_read_attr(eff, "tools_key");
    if (rm2_is_raise(tools_key)) { json_free(key); json_free(sess_raw); return tools_key; }
    bool same = rm2_json_eq(tools_key, key);
    json_free(tools_key);
    if (same) { json_free(key); json_free(sess_raw); return NULL; }
    /* snapshot = eff.tool_snapshot(agent, tools_for_api) */
    json_t *tsa = json_object();
    rm2_arg_j(tsa, "agent", agent);
    rm2_arg_j(tsa, "tools_for_api", tools_for_api);
    json_t *snapshot = rm2_call(io, "eff.tool_snapshot", tsa);
    if (rm2_is_raise(snapshot)) { json_free(key); json_free(sess_raw); return snapshot; }
    /* with session.lock: if session.closing: return */
    closing = rm2_read_attr(session, "closing");
    if (rm2_is_raise(closing)) { json_free(snapshot); json_free(key); json_free(sess_raw); return closing; }
    is_closing = rm2_truth(closing);
    json_free(closing);
    if (is_closing) { json_free(snapshot); json_free(key); json_free(sess_raw); return NULL; }
    /* session.efficiency.tools_key = key */
    json_set((json_t *)eff, "tools_key", json_copy(key));
    /* changed = session.efficiency.tools.observe(*snapshot) */
    const json_t *tools = NULL;
    bad = rm2_method_recv(eff, "tools", "observe", &tools);
    if (bad) { json_free(snapshot); json_free(key); json_free(sess_raw); return bad; }
    json_t *oba = json_object();
    json_set(oba, "args", snapshot ? snapshot : json_null());   /* ownership transfer; *splat */
    json_t *changed_r = rm2_call(io, "efficiency.tools.observe", oba);
    if (rm2_is_raise(changed_r)) { json_free(key); json_free(sess_raw); return changed_r; }
    bool changed = rm2_truth(changed_r);
    json_free(changed_r);
    /* route = model_.model_route(getattr(agent, "provider", None), getattr(agent, "model", None)) */
    json_t *mra = json_object();
    rm2_arg_j(mra, "provider", rm2_get(agent, "provider"));
    rm2_arg_j(mra, "model", rm2_get(agent, "model"));
    json_t *route = rm2_call(io, "model.model_route", mra);
    if (rm2_is_raise(route)) { json_free(key); json_free(sess_raw); return route; }
    /* if changed and self._announce_cold(self._cold_key(session_id)):
     *     self._emit_rows(session, [eff.cache_break_row("toolset_change", route)]) */
    if (changed) {
        json_t *ka = json_object();
        rm2_arg_s(ka, "session_id", session_id);
        json_t *ck = rm2_call(io, "rt._cold_key", ka);
        if (rm2_is_raise(ck)) { json_free(route); json_free(key); json_free(sess_raw); return ck; }
        json_t *aa = json_object();
        rm2_arg_j(aa, "key", ck);
        json_free(ck);
        json_t *announced = rm2_call(io, "rt._announce_cold", aa);
        if (rm2_is_raise(announced)) { json_free(route); json_free(key); json_free(sess_raw); return announced; }
        bool fresh = rm2_truth(announced);
        json_free(announced);
        if (fresh) {
            json_t *cba = json_object();
            rm2_arg_s(cba, "cause", "toolset_change");
            rm2_arg_j(cba, "route", route);
            json_t *row = rm2_call(io, "eff.cache_break_row", cba);
            if (rm2_is_raise(row)) { json_free(route); json_free(key); json_free(sess_raw); return row; }
            json_t *rows = json_array();
            json_append(rows, row ? row : json_null());
            json_t *era = json_object();
            rm2_arg_j(era, "session", session);
            json_set(era, "rows", rows);
            json_t *r = rm2_call(io, "rt._emit_rows", era);
            if (rm2_is_raise(r)) { json_free(route); json_free(key); json_free(sess_raw); return r; }
            json_free(r);
        }
    }
    json_free(route);
    json_free(key);
    json_free(sess_raw);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. rotate_segment (method) — compression hand-off: link the new segment into
 * the conversation's lineage, share spent turns, retire the idle old segment.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_rotate_segment @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.rotate_segment */
static json_t *rm2_rotate_segment(rm2_io_t *io, const json_t *in, json_t *st)
{
    const char *old_id = json_get_str(in, "old_id", "");
    const char *new_id = json_get_str(in, "new_id", "");
    /* old = self._session({"session_id": old_id}) */
    json_t *event = json_object();
    json_set(event, "session_id", json_string(old_id));
    json_t *sa = json_object();
    json_set(sa, "event", event);
    json_t *old_raw = rm2_call(io, "rt._session", sa);
    if (rm2_is_raise(old_raw)) return old_raw;
    const json_t *old = rm2_session_ref(old_raw, st);
    /* with self._lineage_lock: */
    json_t *lineage_of = rm2_get(st, "_lineage_of");
    json_t *lineages = rm2_get(st, "_lineages");
    const json_t *keynode = rm2_get(lineage_of, old_id);
    char *key = NULL;   /* owned */
    if (rm2_is_null(keynode)) {
        if (rm2_is_null(old)) { json_free(old_raw); return NULL; }
        key = strdup(old_id);
        /* self._lineages[key] = _PeakLineage(open={old_id}, run=old.route_run,
         *     spent=old.efficiency.spent, connectors=old.connectors)
         * default factories fire in dataclass field order: peak, tools, tally. */
        json_t *peak = rm2_call(io, "model.ModelSessionState", json_object());
        if (rm2_is_raise(peak)) { free(key); json_free(old_raw); return peak; }
        json_t *tools = rm2_call(io, "eff.ToolUsage", json_object());
        if (rm2_is_raise(tools)) { json_free(peak); free(key); json_free(old_raw); return tools; }
        json_t *tally = rm2_call(io, "engagement.SessionTally", json_object());
        if (rm2_is_raise(tally)) { json_free(tools); json_free(peak); free(key); json_free(old_raw); return tally; }
        json_t *route_run = rm2_read_attr(old, "route_run");
        if (rm2_is_raise(route_run)) { json_free(tally); json_free(tools); json_free(peak); free(key); json_free(old_raw); return route_run; }
        const json_t *old_eff = NULL;
        json_t *bad = rm2_method_recv(old, "efficiency", "spent", &old_eff);
        if (bad) { json_free(route_run); json_free(tally); json_free(tools); json_free(peak); free(key); json_free(old_raw); return bad; }
        json_t *spent = rm2_read_attr(old_eff, "spent");
        if (rm2_is_raise(spent)) { json_free(route_run); json_free(tally); json_free(tools); json_free(peak); free(key); json_free(old_raw); return spent; }
        json_t *connectors = rm2_read_attr(old, "connectors");
        if (rm2_is_raise(connectors)) { json_free(spent); json_free(route_run); json_free(tally); json_free(tools); json_free(peak); free(key); json_free(old_raw); return connectors; }
        /* dataclass __dict__ order: open, peak, tools, tally, run, spent, connectors */
        json_t *lin = json_object();
        json_t *open = json_array();
        json_append(open, json_string(old_id));
        json_set(lin, "open", open);
        json_set(lin, "peak", peak ? peak : json_null());       /* ownership transfer */
        json_set(lin, "tools", tools ? tools : json_null());
        json_set(lin, "tally", tally ? tally : json_null());
        json_set(lin, "run", route_run ? route_run : json_null());
        json_set(lin, "spent", spent ? spent : json_null());
        json_set(lin, "connectors", connectors ? connectors : json_null());
        json_set(lineages, key, lin);
        json_set(lineage_of, old_id, json_string(key));
    } else {
        key = strdup(rm2_str(keynode) ? rm2_str(keynode) : "");
    }
    /* self._lineages[key].open.add(new_id) */
    json_t *lineage = rm2_get(lineages, key);
    if (!lineage) {
        char *m = rm2_xfmt("'%s'", key);
        json_t *r = rm2_mkraise("KeyError", m ? m : "");
        free(m); free(key); json_free(old_raw);
        return r;
    }
    json_t *open = rm2_get(lineage, "open");
    if (rm2_is_arr(open)) rm2_set_add_str(open, new_id);
    /* self._lineage_of[new_id] = key */
    json_set(lineage_of, new_id, json_string(key));
    /* overflow = list(self._lineages)[:-_MAX_LINEAGES] */
    size_t nlin = rm2_is_obj(lineages) ? lineages->c.count : 0;
    size_t nover = nlin > 512 ? nlin - 512 : 0;
    char **overflow = NULL;
    if (nover) {
        overflow = (char **)malloc(nover * sizeof(char *));
        if (overflow) for (size_t i = 0; i < nover; i++) overflow[i] = strdup(lineages->c.keys[i]);
    }
    free(key);
    /* new = self._session({"session_id": new_id}) */
    event = json_object();
    json_set(event, "session_id", json_string(new_id));
    sa = json_object();
    json_set(sa, "event", event);
    json_t *new_raw = rm2_call(io, "rt._session", sa);
    if (rm2_is_raise(new_raw)) {
        for (size_t i = 0; i < nover; i++) free(overflow[i]);
        free(overflow); json_free(old_raw); return new_raw;
    }
    const json_t *new_sess = rm2_session_ref(new_raw, st);
    /* if new is not None: self._share_spent(new) */
    if (!rm2_is_null(new_sess)) {
        json_t *sha = json_object();
        rm2_arg_j(sha, "session", new_sess);
        json_t *r = rm2_call(io, "rt._share_spent", sha);
        if (rm2_is_raise(r)) {
            for (size_t i = 0; i < nover; i++) free(overflow[i]);
            free(overflow); json_free(new_raw); json_free(old_raw); return r;
        }
        json_free(r);
    }
    /* if old is not None:
     *     with old.lock: old.retiring = not old.closing; idle = old.retiring and not old.tasks
     *     if idle: self.close_session({"session_id": old_id}) */
    if (!rm2_is_null(old)) {
        json_t *closing = rm2_read_attr(old, "closing");
        if (rm2_is_raise(closing)) {
            for (size_t i = 0; i < nover; i++) free(overflow[i]);
            free(overflow); json_free(new_raw); json_free(old_raw); return closing;
        }
        bool retiring = !rm2_truth(closing);
        json_free(closing);
        json_set((json_t *)old, "retiring", json_bool(retiring));
        json_t *tasks = rm2_read_attr(old, "tasks");
        if (rm2_is_raise(tasks)) {
            for (size_t i = 0; i < nover; i++) free(overflow[i]);
            free(overflow); json_free(new_raw); json_free(old_raw); return tasks;
        }
        bool idle = retiring && !rm2_truth(tasks);
        json_free(tasks);
        if (idle) {
            json_t *ce = json_object();
            json_set(ce, "session_id", json_string(old_id));
            json_t *csa = json_object();
            json_set(csa, "event", ce);
            json_t *r = rm2_call(io, "rt.close_session", csa);
            if (rm2_is_raise(r)) {
                for (size_t i = 0; i < nover; i++) free(overflow[i]);
                free(overflow); json_free(new_raw); json_free(old_raw); return r;
            }
            json_free(r);
        }
    }
    /* for stale in overflow: self._flush_lineage(stale) */
    for (size_t i = 0; i < nover; i++) {
        json_t *fa = json_object();
        rm2_arg_s(fa, "key", overflow[i]);
        json_t *r = rm2_call(io, "rt._flush_lineage", fa);
        free(overflow[i]);
        if (rm2_is_raise(r)) {
            for (size_t j = i + 1; j < nover; j++) free(overflow[j]);
            free(overflow); json_free(new_raw); json_free(old_raw); return r;
        }
        json_free(r);
    }
    free(overflow);
    json_free(new_raw);
    json_free(old_raw);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 7. _emit_session_summary — one session.count + context-peak row per closed
 * top-level conversation, its tool usage, plus a quick-abandon friction.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_emit_session_summary @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._emit_session_summary */
static json_t *rm2_emit_session_summary(rm2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    const json_t *session = rm2_get(st, "session");
    /* start = session.start_fields */
    json_t *start = rm2_read_attr(session, "start_fields");
    if (rm2_is_raise(start)) return start;
    /* counted = bool(session.turns) and start is not None and start.get("entrypoint") != "delegated" */
    json_t *turns = rm2_read_attr(session, "turns");
    if (rm2_is_raise(turns)) { json_free(start); return turns; }
    bool counted = rm2_truth(turns);
    json_free(turns);
    if (counted && !rm2_is_null(start)) {
        const json_t *ep = rm2_get(start, "entrypoint");
        if (rm2_is_str(ep) && rm2_str_eq(ep->str_val, "delegated")) counted = false;
    } else if (rm2_is_null(start)) {
        counted = false;
    }
    /* tally = engagement_.SessionTally(...9 args...) if counted else None */
    json_t *tally = NULL;   /* owned */
    if (counted) {
        json_t *sf = rm2_read_attr(session, "start_fields");
        if (rm2_is_raise(sf)) { json_free(start); return sf; }
        json_t *tn = rm2_read_attr(session, "turns");
        if (rm2_is_raise(tn)) { json_free(sf); json_free(start); return tn; }
        json_t *ft = rm2_read_attr(session, "failed_turns");
        if (rm2_is_raise(ft)) { json_free(tn); json_free(sf); json_free(start); return ft; }
        json_t *lo = rm2_read_attr(session, "last_outcome");
        if (rm2_is_raise(lo)) { json_free(ft); json_free(tn); json_free(sf); json_free(start); return lo; }
        json_t *fns = rm2_read_attr(session, "first_turn_ns");
        if (rm2_is_raise(fns)) { json_free(lo); json_free(ft); json_free(tn); json_free(sf); json_free(start); return fns; }
        json_t *lns = rm2_read_attr(session, "last_turn_ns");
        if (rm2_is_raise(lns)) { json_free(fns); json_free(lo); json_free(ft); json_free(tn); json_free(sf); json_free(start); return lns; }
        json_t *ac = rm2_read_attr(session, "api_calls");
        if (rm2_is_raise(ac)) { json_free(lns); json_free(fns); json_free(lo); json_free(ft); json_free(tn); json_free(sf); json_free(start); return ac; }
        json_t *tct = rm2_read_attr(session, "tool_call_total");
        if (rm2_is_raise(tct)) { json_free(ac); json_free(lns); json_free(fns); json_free(lo); json_free(ft); json_free(tn); json_free(sf); json_free(start); return tct; }
        json_t *rp = rm2_read_attr(session, "replies");
        if (rm2_is_raise(rp)) { json_free(tct); json_free(ac); json_free(lns); json_free(fns); json_free(lo); json_free(ft); json_free(tn); json_free(sf); json_free(start); return rp; }
        json_t *ta = json_object();
        rm2_arg_j(ta, "start_fields", sf); json_free(sf);
        rm2_arg_j(ta, "turns", tn); json_free(tn);
        rm2_arg_j(ta, "failed_turns", ft); json_free(ft);
        rm2_arg_j(ta, "last_outcome", lo); json_free(lo);
        rm2_arg_j(ta, "first_turn_ns", fns); json_free(fns);
        rm2_arg_j(ta, "last_turn_ns", lns); json_free(lns);
        rm2_arg_j(ta, "api_calls", ac); json_free(ac);
        rm2_arg_j(ta, "tool_call_total", tct); json_free(tct);
        rm2_arg_j(ta, "replies", rp); json_free(rp);
        tally = rm2_call(io, "engagement.SessionTally", ta);
        if (rm2_is_raise(tally)) { json_free(start); return tally; }
    }
    /* tools = session.efficiency.tools if counted and model_.attended(start) else eff.ToolUsage() */
    json_t *tools = NULL;   /* owned when fresh; borrowed flag below */
    bool tools_borrowed = false;
    bool use_session_tools = false;
    if (counted) {
        json_t *aa = json_object();
        rm2_arg_j(aa, "start_fields", start);
        json_t *attended = rm2_call(io, "model.attended", aa);
        if (rm2_is_raise(attended)) { json_free(tally); json_free(start); return attended; }
        use_session_tools = rm2_truth(attended);
        json_free(attended);
    }
    if (use_session_tools) {
        const json_t *eff = NULL;
        json_t *bad = rm2_method_recv(session, "efficiency", "tools", &eff);
        if (bad) { json_free(tally); json_free(start); return bad; }
        json_t *t = rm2_read_attr(eff, "tools");
        if (rm2_is_raise(t)) { json_free(tally); json_free(start); return t; }
        /* borrow the state carrier sub-object so the surface write echoes */
        tools = rm2_get(eff, "tools");
        tools_borrowed = true;
        json_free(t);
    } else {
        tools = rm2_call(io, "eff.ToolUsage", json_object());
        if (rm2_is_raise(tools)) { json_free(tally); json_free(start); return tools; }
    }
    /* tools.surface = (start or {}).get("execution_surface", "unknown") */
    if (rm2_is_null(tools)) {
        if (!tools_borrowed) json_free(tools);
        json_free(tally); json_free(start);
        return rm2_mkraise("AttributeError",
            "'NoneType' object has no attribute 'surface' and no __dict__ for setting new attributes");
    }
    json_t *surface = json_string("unknown");
    if (rm2_truth(start) && rm2_is_obj(start)) {
        const json_t *es = rm2_get(start, "execution_surface");
        if (es) { json_free(surface); surface = json_copy(es); }
    }
    json_set(tools, "surface", surface);   /* ownership transfer; borrowed carrier mutated in place */
    /* merged = self._close_segment(session.session_id, session.model_state if counted else None, tally, tools) */
    json_t *sid = rm2_read_attr(session, "session_id");
    if (rm2_is_raise(sid)) { if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return sid; }
    json_t *seg_state = NULL;
    if (counted) {
        seg_state = rm2_read_attr(session, "model_state");
        if (rm2_is_raise(seg_state)) { json_free(sid); if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return seg_state; }
    }
    json_t *csa = json_object();
    rm2_arg_j(csa, "session_id", sid);
    json_free(sid);
    rm2_arg_j(csa, "segment_state", seg_state);
    json_free(seg_state);
    rm2_arg_j(csa, "tally", tally);
    rm2_arg_j(csa, "tools", tools);
    json_t *merged = rm2_call(io, "rt._close_segment", csa);
    if (rm2_is_raise(merged)) { if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return merged; }
    /* abandoned = session.model_state.quick_abandon_route(monotonic_ns()) if counted and not session.retiring else None */
    json_t *abandoned = NULL;   /* owned */
    if (counted) {
        json_t *retiring = rm2_read_attr(session, "retiring");
        if (rm2_is_raise(retiring)) { json_free(merged); if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return retiring; }
        bool is_retiring = rm2_truth(retiring);
        json_free(retiring);
        if (!is_retiring) {
            json_t *now = rm2_call(io, "monotonic_ns", json_object());
            if (rm2_is_raise(now)) { json_free(merged); if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return now; }
            const json_t *ms = NULL;
            json_t *bad = rm2_method_recv(session, "model_state", "quick_abandon_route", &ms);
            if (bad) { json_free(now); json_free(merged); if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return bad; }
            json_t *qa = json_object();
            rm2_arg_j(qa, "now_ns", now);
            json_free(now);
            abandoned = rm2_call(io, "model_state.quick_abandon_route", qa);
            if (rm2_is_raise(abandoned)) { json_free(merged); if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return abandoned; }
        }
    }
    /* friction = [(MODEL_FRICTION_MARK, model_.friction_fields("quick_abandon", abandoned))]
     *            if abandoned is not None and model_.attended(start) else [] */
    json_t *friction = json_array();
    if (!rm2_is_null(abandoned)) {
        json_t *aa = json_object();
        rm2_arg_j(aa, "start_fields", start);
        json_t *attended = rm2_call(io, "model.attended", aa);
        if (rm2_is_raise(attended)) { json_free(abandoned); json_free(friction); json_free(merged); if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return attended; }
        bool att = rm2_truth(attended);
        json_free(attended);
        if (att) {
            json_t *fa = json_object();
            rm2_arg_s(fa, "signal", "quick_abandon");
            rm2_arg_j(fa, "route", abandoned);
            json_t *fields = rm2_call(io, "model.friction_fields", fa);
            if (rm2_is_raise(fields)) { json_free(abandoned); json_free(friction); json_free(merged); if (!tools_borrowed) json_free(tools); json_free(tally); json_free(start); return fields; }
            json_t *row = json_array();
            json_append(row, json_string("hermes.model_friction"));
            json_append(row, fields ? fields : json_null());   /* ownership transfer */
            json_append(friction, row);
        }
    }
    json_free(abandoned);
    /* self._emit_summary_rows(session, *merged, *friction) */
    json_t *esa = json_object();
    rm2_arg_j(esa, "session", session);
    rm2_arg_j(esa, "peak", json_get(merged, 0));
    rm2_arg_j(esa, "tally", json_get(merged, 1));
    rm2_arg_j(esa, "tools", json_get(merged, 2));
    json_set(esa, "extra", friction);   /* ownership transfer */
    json_free(merged);
    json_t *r = rm2_call(io, "rt._emit_summary_rows", esa);
    if (!tools_borrowed) json_free(tools);
    json_free(tally);
    json_free(start);
    if (rm2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 8. record_process_mark (method) — a process-level fact under the scope stack.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_record_process_mark @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_process_mark */
static json_t *rm2_record_process_mark(rm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *mark = json_get_str(in, "mark", "");
    const json_t *data = rm2_get(in, "data");
    /* metadata kwarg evaluates BEFORE the _with_scope_stack seam fires */
    json_t *meta = rm2_call(io, "rt._event_metadata", json_object());
    if (rm2_is_raise(meta)) return meta;
    json_t *cb = rm2_callable("relay.scope.event");
    json_t *a = json_object();
    json_set(a, "callback", cb);
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(mark));
    json_set(a, "args", args_arr);
    json_t *kwargs_obj = json_object();
    rm2_arg_j(kwargs_obj, "data", data);
    rm2_arg_j(kwargs_obj, "metadata", meta);
    json_free(meta);
    json_set(a, "kwargs", kwargs_obj);
    json_t *r = rm2_call(io, "rt._with_scope_stack", a);
    if (rm2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 9. record_process_marks_saved (method) — emit marks with a commit ticket,
 * flush, and return how many settled without a store error.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_record_process_marks_saved @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_process_marks_saved */
static json_t *rm2_record_process_marks_saved(rm2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *marks = rm2_get(in, "marks");
    /* ticket = uuid.uuid4().hex */
    json_t *u = rm2_call(io, "uuid.uuid4", json_object());
    if (rm2_is_raise(u)) return u;
    json_t *ticket = rm2_read_attr(u, "hex");
    json_free(u);
    if (rm2_is_raise(ticket)) return ticket;
    /* metadata = {**self._event_metadata(), contract.COMMIT_TICKET_KEY: ticket} */
    json_t *meta = rm2_call(io, "rt._event_metadata", json_object());
    if (rm2_is_raise(meta)) { json_free(ticket); return meta; }
    if (rm2_is_null(meta)) {
        json_free(meta); json_free(ticket);
        return rm2_mkraise("TypeError", "'NoneType' object is not a mapping");
    }
    json_set(meta, "hermes.shared_metrics.commit_ticket", json_copy(ticket));
    /* for mark, data in marks: self._with_scope_stack(self.relay.scope.event, mark, data=data, metadata=metadata) */
    if (rm2_is_null(marks)) {
        json_free(meta); json_free(ticket);
        return rm2_mkraise("TypeError", "'NoneType' object is not iterable");
    }
    if (rm2_is_arr(marks)) {
        for (size_t i = 0; i < marks->c.count; i++) {
            const json_t *pair = marks->c.items[i];
            size_t pn = rm2_is_arr(pair) ? json_len(pair) : 0;
            if (pn < 2) {
                char *m = rm2_xfmt("not enough values to unpack (expected 2, got %zu)", pn);
                json_t *r = rm2_mkraise("ValueError", m ? m : "");
                free(m); json_free(meta); json_free(ticket); return r;
            }
            if (pn > 2) {
                char *m = rm2_xfmt("too many values to unpack (expected 2, got %zu)", pn);
                json_t *r = rm2_mkraise("ValueError", m ? m : "");
                free(m); json_free(meta); json_free(ticket); return r;
            }
            const json_t *mark = json_get(pair, 0);
            const json_t *data = json_get(pair, 1);
            json_t *cb = rm2_callable("relay.scope.event");
            json_t *a = json_object();
            json_set(a, "callback", cb);
            json_t *args_arr = json_array();
            json_append(args_arr, mark ? json_copy(mark) : json_null());
            json_set(a, "args", args_arr);
            json_t *kwargs_obj = json_object();
            rm2_arg_j(kwargs_obj, "data", data);
            rm2_arg_j(kwargs_obj, "metadata", meta);
            json_set(a, "kwargs", kwargs_obj);
            json_t *r = rm2_call(io, "rt._with_scope_stack", a);
            if (rm2_is_raise(r)) { json_free(meta); json_free(ticket); return r; }
            json_free(r);
        }
    }
    json_free(meta);
    /* self.relay.subscribers.flush() */
    json_t *fl = rm2_call(io, "relay.subscribers.flush", json_object());
    if (rm2_is_raise(fl)) { json_free(ticket); return fl; }
    json_free(fl);
    /* return self.subscriber.take_saved(ticket) */
    const json_t *subscriber = NULL;
    json_t *bad = rm2_method_recv(st, "subscriber", "take_saved", &subscriber);
    if (bad) { json_free(ticket); return bad; }
    json_t *tsa = json_object();
    rm2_arg_j(tsa, "ticket", ticket);
    json_free(ticket);
    return rm2_call(io, "subscriber.take_saved", tsa);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 10. still_consented (closure in _run_send_pass) — re-read consent so revoking
 * `send` stops an in-flight pass. Returns send itself when falsy (live `and`),
 * else the endpoint-equality bool.
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rm2_still_consented @ hermes_cli/observability/relay_shared_metrics.py:still_consented */
static json_t *rm2_still_consented(rm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *endpoint = json_get_str(in, "endpoint", "");
    /* resolved = _resolved_send_config() */
    json_t *resolved = rm2_call(io, "_resolved_send_config", json_object());
    if (rm2_is_raise(resolved)) return resolved;
    /* return resolved.send and resolved.endpoint == endpoint */
    json_t *send = rm2_read_attr(resolved, "send");
    if (rm2_is_raise(send)) { json_free(resolved); return send; }
    if (!rm2_truth(send)) {
        json_free(resolved);
        return send;   /* the falsy send value itself (None/0/""/false) */
    }
    json_free(send);
    json_t *ep = rm2_read_attr(resolved, "endpoint");
    json_free(resolved);
    if (rm2_is_raise(ep)) return ep;
    bool same = rm2_str_eq(rm2_str(ep), endpoint);
    json_free(ep);
    return json_bool(same);
}