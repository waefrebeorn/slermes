/* relay_metrics_flow3.c — FLOW port of the FIFTH-wave remainder of
 * hermes_cli/observability/relay_shared_metrics.py (the relay-metrics module;
 * the task brief named "relay_metrics.py", which does not exist in the quarry —
 * per the skill's defining-module rule every PoP below names the REAL defining
 * module). The claimed siblings live in relay_shared_metrics.c / _flow.c /
 * _gaps, src/cli/relay_shared_metrics_flow2.c, src/cli/relay_metrics_flow.c and
 * src/cli/relay_metrics_flow2.c and are SEAMED here through the injected
 * universal seam rm3_io_t. This is a standalone differential oracle over an
 * independently re-derived, coherent slice (text/identity/key resolution plus
 * the guarded callback/exception core); the older claims stay in force.
 *
 * Decision-sequence port (9 functions): _text / _retry_ordinal / _session_pair
 * / _identities_compatible (pure leaves), _event_matches_task_turn /
 * _existing_model_call_key (task+session correlation), and the guarded callback
 * core _guarded -> _safe -> _export (composed REAL). Every collaborator rides
 * the universal seam; the harness scripts results and records the ordered
 * trace the oracle diffs against LIVE relay_shared_metrics.py under the same
 * scripts. The FULL builtin exception-parent table is carried so `_guarded`
 * swallows exactly the Exception family (including ExceptionGroup) and lets
 * every BaseException-only family pass through, as live does.
 *
 * Representation contract (shared with tests/oracle_relay_metrics_flow3.py):
 *   - raise specs are VALUES {"__raise__": kind, "message": msg} threaded
 *     through every helper; an uncaught one becomes the case ret.
 *   - fail-closed: an absent / exhausted seam script returns a RuntimeError
 *     raise spec ("no script for <name>"), recorded as the event ret BEFORE
 *     propagation.
 *   - object doubles are attribute dicts read via rm3_get; tuple -> array;
 *     None -> NULL pointer / JSON null; integral floats collapse to ints.
 *   - callable doubles {"__callable__": "<seam>"} fire that seam with
 *     {"args": [...], "kwargs": {...}} when invoked. store callbacks and
 *     self._send_exported_packages project to fixed live-name markers.
 *   - a bare JSON array script is a per-call QUEUE; a list RESULT is wrapped
 *     {"__script__": true, "ret": [...]}; a scripted null returns None.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hermes_json.h"

typedef struct {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} rm3_io_t;

/* ---------------- small helpers ---------------- */
static json_t *rm3_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT && k) ? json_obj_get((json_t *)o, k) : NULL;
}
static const char *rm3_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static int rm3_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default:          return 0;
    }
}
static json_t *rm3_int(long long n) { return json_number((double)n); }

static char *rm3_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) return NULL;
    va_start(ap, fmt);
    vsnprintf(b, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return b;
}

static json_t *rm3_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind ? kind : "RuntimeError"));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static int rm3_is_raise(const json_t *v)
{
    return v && v->type == JSON_OBJECT && rm3_get(v, "__raise__") != NULL;
}
static const char *rm3_raise_kind(const json_t *v)
{
    return rm3_str(rm3_get(v, "__raise__"));
}

/* Python str(event.get(key) or "") into a caller buffer. */
static void rm3_text_into(const json_t *v, char *buf, size_t n)
{
    buf[0] = '\0';
    if (!v || !rm3_truthy(v)) return;
    if (v->type == JSON_STRING) {
        snprintf(buf, n, "%s", v->str_val ? v->str_val : "");
    } else if (v->type == JSON_BOOL) {
        snprintf(buf, n, "%s", v->bool_val ? "True" : "False");
    } else if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
            snprintf(buf, n, "%lld", (long long)v->num_val);
        else
            snprintf(buf, n, "%.17g", v->num_val);
    }
}
static int rm3_arr_has_str(const json_t *a, const char *s)
{
    if (!a || a->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(a); i++)
        if (strcmp(rm3_str(json_get(a, i)), s) == 0) return 1;
    return 0;
}

/* ---------------- FULL builtin exception-parent hierarchy ----------------
 * parent == NULL means BaseException's root. ExceptionGroup is an Exception;
 * BaseExceptionGroup / CancelledError / SystemExit / KeyboardInterrupt /
 * GeneratorExit are BaseException-only. The walk decides `except Exception`.
 */
typedef struct { const char *name; const char *parent; } rm3_exc_entry_t;
static const rm3_exc_entry_t RM3_EXC_TABLE[] = {
    {"BaseException", NULL},
    {"BaseExceptionGroup", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"Exception", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"ExceptionGroup", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"ModuleNotFoundError", "ImportError"},
    {"UnboundLocalError", "NameError"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
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
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"WindowsError", "OSError"},
};
#define RM3_EXC_N (sizeof(RM3_EXC_TABLE) / sizeof(RM3_EXC_TABLE[0]))

static int rm3_exc_is_exception(const char *kind)
{
    const char *cur = kind;
    for (int hop = 0; hop < 64 && cur; hop++) {
        if (strcmp(cur, "Exception") == 0) return 1;
        if (strcmp(cur, "BaseException") == 0) return 0;
        const char *par = NULL;
        int found = 0;
        for (size_t j = 0; j < RM3_EXC_N; j++) {
            if (strcmp(RM3_EXC_TABLE[j].name, cur) == 0) {
                par = RM3_EXC_TABLE[j].parent;
                found = 1;
                break;
            }
        }
        if (!found) return 0; /* unknown kind: not an Exception in our domain */
        cur = par;
    }
    return 0;
}

/* ---------------- universal seam / callable doubles ---------------- */
static json_t *rm3_call(rm3_io_t *io, const char *name, const json_t *args)
{
    return io->call(io->ctx, name, args);
}
static json_t *rm3_callable(rm3_io_t *io, const json_t *cb,
                            const json_t *args_arr, const json_t *kwargs)
{
    if (!cb || cb->type != JSON_OBJECT)
        return rm3_mkraise("TypeError", "object is not callable");
    const char *nm = rm3_str(rm3_get(cb, "__callable__"));
    if (!nm[0]) return rm3_mkraise("TypeError", "object is not callable");
    json_t *payload = json_object();
    json_set(payload, "args", args_arr ? json_copy((json_t *)args_arr) : json_array());
    json_set(payload, "kwargs", kwargs ? json_copy((json_t *)kwargs) : json_object());
    json_t *r = rm3_call(io, nm, payload);
    json_free(payload);
    return r;
}

/* ---------------- PORTED FUNCTIONS (static rm3_*) ---------------- */

/* PoP: rm3_text @ hermes_cli/observability/relay_shared_metrics.py:_text */
static json_t *rm3_text(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    char b[512];
    rm3_text_into(rm3_get(rm3_get(in, "event"), rm3_str(rm3_get(in, "key"))),
                  b, sizeof b);
    return json_string(b);
}

/* PoP: rm3_retry_ordinal @ hermes_cli/observability/relay_shared_metrics.py:_retry_ordinal */
static json_t *rm3_retry_ordinal(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *v = rm3_get(rm3_get(in, "event"), "retry_count");
    if (v && v->type == JSON_NUMBER && v->num_val > 0 &&
        v->num_val == floor(v->num_val))
        return rm3_int((long long)v->num_val);
    return rm3_int(0);
}

/* PoP: rm3_session_pair @ hermes_cli/observability/relay_shared_metrics.py:_session_pair */
static json_t *rm3_session_pair(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *ev = rm3_get(in, "event");
    const char *key = rm3_str(rm3_get(in, "key"));
    char sid[256], val[256];
    rm3_text_into(rm3_get(ev, "session_id"), sid, sizeof sid);
    rm3_text_into(rm3_get(ev, key), val, sizeof val);
    if (sid[0] && val[0]) {
        json_t *r = json_array();
        json_append(r, json_string(sid));
        json_append(r, json_string(val));
        return r;
    }
    return json_null();
}

/* PoP: rm3_identities_compatible @ hermes_cli/observability/relay_shared_metrics.py:_identities_compatible */
static json_t *rm3_identities_compatible(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *c = rm3_get(in, "candidate");
    const json_t *o = rm3_get(in, "observed");
    const char *c2 = rm3_str(json_get(c, 2));
    const char *o2 = rm3_str(json_get(o, 2));
    if (!o2[0] || strcmp(c2, o2) != 0) return json_bool(0);
    for (int i = 0; i < 2; i++) {
        const char *l = rm3_str(json_get(c, i));
        const char *r = rm3_str(json_get(o, i));
        if (l[0] && r[0] && strcmp(l, r) != 0) return json_bool(0);
    }
    return json_bool(1);
}

/* PoP: rm3_event_matches_task_turn @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._event_matches_task_turn */
static json_t *rm3_event_matches_task_turn(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *task = rm3_get(in, "task");
    const json_t *ev = rm3_get(in, "event");
    char tid[256];
    rm3_text_into(rm3_get(ev, "turn_id"), tid, sizeof tid);
    if (!tid[0]) return json_bool(1);
    if (rm3_arr_has_str(rm3_get(task, "retired_turn_ids"), tid)) return json_bool(0);
    const json_t *turns = rm3_get(task, "turn_ids");
    if (turns && json_len(turns) > 0 && !rm3_arr_has_str(turns, tid)) return json_bool(0);
    return json_bool(1);
}

/* PoP: rm3_existing_model_call_key @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._existing_model_call_key */
static json_t *rm3_existing_model_call_key(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *sess = rm3_get(in, "session");
    const json_t *ev = rm3_get(in, "event");
    char rid[256], tid[256];
    rm3_text_into(rm3_get(ev, "api_request_id"), rid, sizeof rid);
    if (!rid[0]) return json_null();
    rm3_text_into(rm3_get(ev, "task_id"), tid, sizeof tid);
    const json_t *mcalls = rm3_get(sess, "model_calls");
    size_t n = (mcalls && mcalls->type == JSON_ARRAY) ? json_len(mcalls) : 0;
    int present = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *mc = json_get(mcalls, i);
        if (json_len(mc) >= 2 &&
            strcmp(rm3_str(json_get(mc, 0)), tid) == 0 &&
            strcmp(rm3_str(json_get(mc, 1)), rid) == 0) { present = 1; break; }
    }
    if (present || tid[0]) {
        if (!present) return json_null();
        json_t *r = json_array();
        json_append(r, json_string(tid));
        json_append(r, json_string(rid));
        return r;
    }
    int cnt = 0; const json_t *cand = NULL;
    for (size_t i = 0; i < n; i++) {
        const json_t *mc = json_get(mcalls, i);
        if (json_len(mc) >= 2 && strcmp(rm3_str(json_get(mc, 1)), rid) == 0) {
            cnt++; cand = mc;
        }
    }
    if (cnt == 1 && cand) {
        json_t *r = json_array();
        json_append(r, json_string(rm3_str(json_get(cand, 0))));
        json_append(r, json_string(rm3_str(json_get(cand, 1))));
        return r;
    }
    return json_null();
}

/* PoP: rm3_guarded @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._guarded */
static json_t *rm3_guarded(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *msg = rm3_str(rm3_get(in, "message"));
    json_t *ret = rm3_callable(io, rm3_get(in, "callback"),
                               rm3_get(in, "args"), rm3_get(in, "kwargs"));
    if (rm3_is_raise(ret)) {
        if (rm3_exc_is_exception(rm3_raise_kind(ret))) {
            json_t *pay = json_object();
            json_t *aa = json_array();
            json_append(aa, json_string(msg));
            json_set(pay, "args", aa);
            json_t *kk = json_object();
            json_set(kk, "exc_info", json_bool(1));
            json_set(pay, "kwargs", kk);
            json_t *log = rm3_call(io, "logger.warning", pay);
            json_free(pay);
            if (rm3_is_raise(log)) return log;
            return json_null(); /* None */
        }
        return ret; /* BaseException-only: passes straight through */
    }
    return ret;
}

/* PoP: rm3_safe @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._safe */
static json_t *rm3_safe(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *d = json_copy((json_t *)in);
    json_set(d, "message", json_string("Hermes shared metrics operation failed"));
    json_t *r = rm3_guarded(io, d, st);
    json_free(d);
    return r;
}

/* PoP: rm3_export @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._export */
static json_t *rm3_export(rm3_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *d1 = json_object();
    json_t *cb1 = json_object();
    json_set(cb1, "__callable__", json_string("store.create_and_export_package_if_due"));
    json_set(d1, "callback", cb1);
    json_set(d1, "args", json_array());
    json_set(d1, "kwargs", json_object());
    json_t *exported = rm3_safe(io, d1, st);
    json_free(d1);
    if (rm3_is_raise(exported)) return exported;
    if (exported && exported->type != JSON_NULL) {
        json_t *d2 = json_object();
        json_t *cb2 = json_object();
        json_set(cb2, "__callable__", json_string("_send_exported_packages"));
        json_set(d2, "callback", cb2);
        json_set(d2, "args", json_array());
        json_set(d2, "kwargs", json_object());
        json_t *sent = rm3_safe(io, d2, st);
        json_free(d2);
        if (rm3_is_raise(sent)) return sent;
    }
    return json_null();
}

/* Silence -Wunused-function for helpers used only by some translations when
 * the harness includes this file: the harness references every dispatcher
 * entry, so these are all live. Kept as a compile-time reachability check. */
static void rm3_reachable(void)
{
    (void)rm3_text; (void)rm3_retry_ordinal; (void)rm3_session_pair;
    (void)rm3_identities_compatible; (void)rm3_event_matches_task_turn;
    (void)rm3_existing_model_call_key; (void)rm3_guarded; (void)rm3_safe;
    (void)rm3_export; (void)rm3_xfmt; (void)rm3_exc_is_exception;
}
