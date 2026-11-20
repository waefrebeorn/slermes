/*
 * run_notifications_flow3.c — gateway/run_notifications.py UNCLAIMED-remainder
 * FLOW cores, wave 3 (universal-seam convention, sibling of
 * run_notifications_flow.c / run_notifications_flow2.c).
 *
 * Ports the DECISION SEQUENCES of the queued-lane final send, the home-channel
 * message send, the planned-restart marker replay, the state.db warning
 * broadcast, the api_server self-post wake, the committed-relay adapter proof,
 * the completion profile-scope resolvers, the scoped completion delivery
 * (claim/refund/settle finally), the batch-waiter settlement, the async
 * delegation group delivery wrappers and the live watcher arming: call order,
 * short-circuit branches, early returns, exception->fallback paths, finally
 * blocks and mutation order of the live Python. Every live I/O (sends, scope
 * binding, path probes, durable-claim ops, clocks, logging, queue puts) enters
 * through one universal seam rn3_seams_t.call(ctx, name, args); args is OWNED
 * by rn3_call (freed after the borrowed call); results are NEW json_t with
 * NULL == Python None and {"__raise__":K,"msg":M} == a raised exception.
 * BaseException kinds (SystemExit/KeyboardInterrupt/CancelledError/
 * GeneratorExit/BaseExceptionGroup) pass through every `except Exception`
 * site; except LookupError / WakeNotAccepted / AuthorityRefusal match by the
 * FULL builtin exception hierarchy (rn3_exc_is).
 *
 * Self-contained: libjson only (no cross-object symbols). All helpers static rn3_*.
 *
 * Seam names (the oracle installs recording stubs under the SAME names):
 *   self.<name>            — a claimed/foreign SELF method (instance attribute;
 *                            no self argument recorded). Claimed siblings
 *                            (_build_process_event_source, _settle_durable_claim,
 *                            _completion_delivery_identity,
 *                            _preflight_completion_delivery,
 *                            _completion_identity_seen,
 *                            _mark_completions_delivered_locked,
 *                            _record_coalesced_completion_siblings,
 *                            _served_home_channel_configs,
 *                            _send_home_channel_startup_notifications,
 *                            _home_channel_transports,
 *                            _served_api_server_wake_profile,
 *                            _back_off_refused_completions,
 *                            _run_process_watcher) and foreign collaborators
 *                            (_thread_metadata_for_target,
 *                            _note_restart_notice_boot,
 *                            _open_session_db_for_active_scope,
 *                            _resolve_profile_home_for_source,
 *                            _adapter_for_source,
 *                            _deliver_async_delegation_group_scoped) are SEAMED.
 *   session_store.<name>   — session-store double methods (_generate_session_key,
 *                            lookup_by_session_key).
 *   <live module fn name>  — module-level claimed/foreign functions, verbatim
 *                            (_mark_notify_metadata, _non_conversational_metadata,
 *                            _served_notice_target_key,
 *                            _raw_process_event_session_id,
 *                            _mark_internal_notification,
 *                            diagnostic_process_event, classify_persistence_error,
 *                            profile_cli_selector, get_default_hermes_root,
 *                            profile_scope, store_location,
 *                            describe_storage_failure, t, active_authority,
 *                            committed_route_envelope, restore_native,
 *                            completion_event_scope_or_refuse,
 *                            persist_delegation_delivery, deliver_wake,
 *                            _async_profile_runtime_scope,
 *                            async_launch_profile_scope_if_multiplexed,
 *                            get_hermes_home_override, should_surface_notification,
 *                            restore_completion_route, follow_detached_pin,
 *                            completion_admission, warning_notifications_enabled,
 *                            safe_schedule_threadsafe, atomic_json_write,
 *                            _planned_restart_notification_path).
 *   adapter.send / adapter.send_final_ledgered / transport.send — send hooks.
 *   path.exists / path.read_text / path.unlink — restart-marker Path probes.
 *   future.done / future.set_result — batch-waiter future hooks.
 *   queue.put / loop.is_running / scope_enter / scope_exit — queue, loop, CM.
 *   time.monotonic / logger.debug/info/warning/error/log — clock, logger.
 * asyncio.to_thread(fn, ...) is modeled as the fn seam itself (the thread
 * dispatch is transparent); contextvars.Context().run is transparent likewise.
 *
 * Modeling notes (differential oracle tests/oracle_run_notifications_flow3.py):
 *  - Runner state (self) is injected json_t mutated in place. Platform keys are
 *    the enum .value strings. Adapter doubles are objects whose PRESENT hook
 *    keys model callable methods (send / send_final_ledgered); the marker key
 *    "__base__": true models isinstance(adapter, BasePlatformAdapter). Data
 *    fields are read directly. Source/claim/failure/restored doubles are
 *    SimpleNamespace-shaped (absent attr -> the module-qualified py3.14
 *    AttributeError); home/cfg/transport/adapter/store doubles are AttrDouble-
 *    shaped (absent attr -> "missing attribute '<name>'").
 *  - Ported siblings run inline (real) on both sides: _send_home_channel_message
 *    -> _send_session_db_warning_notifications (via the real present_notification
 *    transcription); _deliver_async_delegation_group ->
 *    _deliver_async_delegation_groups; _settle_batch_waiters is standalone.
 *    _served_api_server_event_scope is an asynccontextmanager: its CALL creates
 *    the CM marker {"scope":"served_api_server","evt":...,"raw_sid":...} with NO
 *    body seams; the body runs on enter (direct-drive flow).
 *  - Scopes are marker objects {"scope": kind, ...}; `async with` fires
 *    scope_enter/scope_exit with {"scope": marker}; {"scope":"nullcontext"}
 *    models contextlib.nullcontext() (no events). An exit raise overrides the
 *    body raise; an enter raise skips exit.
 *  - Sets (delivered/owed targets, inflight identities) travel as arrays with
 *    deep-equality membership; set-iteration positions (delivered_targets write,
 *    skip_targets arg, inflight echo) SORT by compact JSON serialization on both
 *    sides (live set order is hash-randomized).
 *  - Integral floats == ints (libjson has no int/float identity). `is True` /
 *    `is False` are JSON_BOOL-only identity checks; truthiness is the full
 *    Python falsy set. KeyError raise specs carry the RAW key in scripts; the
 *    projected str(exc) (ret/log positions) is the repr'd key (rn3_exc_str).
 *  - send_final_ledgered scripts are 2-element lists [result, extra] (the live
 *    tuple unpack); dict/str unpack shapes are OUTSIDE the fixture domain.
 *    read_text scripts carry canonical json.dumps text (or empty/invalid-start
 *    single-line text for the JSONDecodeError char-0 branch); the lenient
 *    json_parse accept/reject surface beyond that is out of domain. One leading
 *    BOM is stripped after the read_text seam (utf-8-sig), on both sides.
 *  - backups_dir = get_default_hermes_root() / "backups" is a PurePosixPath
 *    component join over the domain {"", ".", "rel", "/abs", "/abs/"} (no "..",
 *    no "//" prefix); the t() seam arg carries as_posix().
 *  - The fixture home carries user_id/scope_id PRESENT (null ok) on the relay
 *    path; one absent-attr case probes the AttrDouble raise. str()/repr()
 *    fixture domain: strings, integral numbers, bools, None. Non-ASCII message
 *    constants (em dash) are hex-escaped literals; fixture INPUTS are ASCII.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "libjson/json.h"

typedef struct rn3_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} rn3_seams_t;

/* ── universal-seam helpers ── */
static json_t *rn3_call(const rn3_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    if (!r && !(s && s->call)) {
        /* no seam installed: fail closed */
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("RuntimeError"));
        json_set(e, "msg", json_string("seam not installed"));
        return e;
    }
    return r;
}
static bool rn3_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *rn3_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *rn3_msg(const json_t *r) {
    const json_t *k = json_obj_get(r, "msg");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *rn3_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *rn3_args(void) { return json_object(); }
static void rn3_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void rn3_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void rn3_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void rn3_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *rn3_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* ── FULL builtin exception hierarchy (py3.14) + asyncio.CancelledError +
 * the two custom quarry kinds. Multiple parents are multiple edges (BFS).
 * except Exception matches iff the kind's ancestry reaches Exception; the
 * BaseException-direct kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 * CancelledError/BaseExceptionGroup) therefore PASS THROUGH every
 * except-Exception site. ── */
typedef struct { const char *child, *parent; } rn3_exc_edge_t;
static const rn3_exc_edge_t rn3_exc_edges[] = {
    {"Exception", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"CancelledError", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ExceptionGroup", "Exception"},
    {"ExceptionGroup", "BaseExceptionGroup"},
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
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
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
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"JSONDecodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    /* quarry custom kinds (stub-module classes in the oracle) */
    {"PersistenceError", "Exception"},
    {"AuthorityRefusal", "PersistenceError"},
    {"AuthorityRefusal", "ValueError"},
    {"WakeNotAccepted", "RuntimeError"},
};
static bool rn3_exc_is(const char *kind, const char *ancestor) {
    if (!kind || !ancestor) return false;
    if (strcmp(kind, ancestor) == 0) return true;
    const char *queue[128];
    size_t head = 0, tail = 0;
    queue[tail++] = kind;
    while (head < tail) {
        const char *cur = queue[head++];
        for (size_t i = 0; i < sizeof(rn3_exc_edges) / sizeof(rn3_exc_edges[0]); i++) {
            if (strcmp(rn3_exc_edges[i].child, cur) != 0) continue;
            const char *p = rn3_exc_edges[i].parent;
            if (strcmp(p, ancestor) == 0) return true;
            bool seen = false;
            for (size_t k = 0; k < tail; k++) if (strcmp(queue[k], p) == 0) { seen = true; break; }
            if (!seen && tail < 128) queue[tail++] = p;
        }
    }
    return false;
}
static bool rn3_is_base_exc(const char *kind) { return !rn3_exc_is(kind, "Exception"); }
/* str(exc) projection of a raise spec: KeyError str is repr(args[0]); scripts
 * carry the RAW key, so the projected message is the repr'd key. */
static char *rn3_repr_str(const char *s);
static char *rn3_exc_str(const json_t *spec) {
    const char *kind = rn3_kind(spec);
    const char *msg = rn3_msg(spec);
    if (strcmp(kind, "KeyError") == 0) return rn3_repr_str(msg);
    return strdup(msg);
}
/* exc as a logger/ret value: {"__raise__": kind, "msg": str(exc)}. */
static json_t *rn3_exc_value(const json_t *spec) {
    json_t *v = json_object();
    json_set(v, "__raise__", json_string(rn3_kind(spec)));
    char *m = rn3_exc_str(spec);
    json_set(v, "msg", json_string(m));
    free(m);
    return v;
}

/* ── Python value helpers ── */
static bool rn3_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool rn3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool rn3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rn3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const char *rn3_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *rn3_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d) return "int";
        return "float";
    }
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    return "dict";
}
static bool rn3_isnum(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double rn3_numval(const json_t *v) {
    return (v->type == JSON_BOOL) ? (v->bool_val ? 1.0 : 0.0) : v->num_val;
}
/* str() of a scalar; containers serialize (outside the str()-position domain). */
static char *rn3_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d && d < 9.2e18 && d > -9.2e18) {
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
            return strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : strdup("");
}
/* str(x or "") — falsy collapses to the empty string, NOT str(None). */
static char *rn3_str_or_empty(const json_t *v) {
    if (!rn3_truth(v)) return strdup("");
    return rn3_pystr(v);
}
/* Python repr for the fixture string domain (quote pick + escapes). */
static char *rn3_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return rn3_pystr(v);
    if (v->type != JSON_STRING) { char *s = json_serialize(v); return s ? s : strdup(""); }
    const char *s = v->str_val ? v->str_val : "";
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3, len = 0;
    char *r = malloc(cap);
    r[len++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\') { r[len++] = '\\'; r[len++] = '\\'; }
        else if (c == (unsigned char)q) { r[len++] = '\\'; r[len++] = (char)c; }
        else if (c == '\n') { r[len++] = '\\'; r[len++] = 'n'; }
        else if (c == '\r') { r[len++] = '\\'; r[len++] = 'r'; }
        else if (c == '\t') { r[len++] = '\\'; r[len++] = 't'; }
        else if (c < 0x20 || c == 0x7F) len += (size_t)snprintf(r + len, cap - len, "\\x%02x", c);
        else r[len++] = (char)c;
    }
    r[len++] = q;
    r[len] = '\0';
    return r;
}
static char *rn3_repr_str(const char *s) {
    json_t *n = json_string(s ? s : "");
    char *r = rn3_repr(n);
    json_free(n);
    return r;
}
/* Deep JSON equality with Python numeric cross-type semantics (True == 1). */
static bool rn3_eq(const json_t *a, const json_t *b) {
    bool an = rn3_is_null(a), bn = rn3_is_null(b);
    if (an || bn) return an && bn;
    if (rn3_isnum(a) && rn3_isnum(b)) return rn3_numval(a) == rn3_numval(b);
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!rn3_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !rn3_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
        default: return false;
    }
}
/* a > b for numeric operands; non-numeric -> the live operand-type TypeError. */
static json_t *rn3_gt(const json_t *a, const json_t *b) {
    if (!rn3_isnum(a) || !rn3_isnum(b)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "'>' not supported between instances of '%s' and '%s'",
                 rn3_typename(a), rn3_typename(b));
        return rn3_raise("TypeError", msg);
    }
    return json_bool(rn3_numval(a) > rn3_numval(b));
}
/* unpack length guard (py3.14 messages); NULL when the array has exactly n items. */
static json_t *rn3_unpack_check(const json_t *arr, size_t n) {
    if (rn3_is_arr(arr) && json_len(arr) == n) return NULL;
    size_t got = rn3_is_arr(arr) ? json_len(arr) : 0;
    char buf[96];
    if (!rn3_is_arr(arr))
        snprintf(buf, sizeof(buf), "cannot unpack non-iterable %s object", rn3_typename(arr));
    else if (got < n)
        snprintf(buf, sizeof(buf), "not enough values to unpack (expected %zu, got %zu)", n, got);
    else
        snprintf(buf, sizeof(buf), "too many values to unpack (expected %zu, got %zu)", n, got);
    return rn3_raise("ValueError", buf);
}
/* group[0] over the list domain. */
static json_t *rn3_index0(const json_t *arr) {
    if (rn3_is_arr(arr)) {
        if (json_len(arr) == 0) return rn3_raise("IndexError", "list index out of range");
        return json_copy(json_get(arr, 0));
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "'%s' object is not subscriptable", rn3_typename(arr));
    return rn3_raise("TypeError", buf);
}
/* malloc'd concatenation of two C strings. */
static char *rn3_concat(const char *a, const char *b) {
    size_t n = strlen(a) + strlen(b) + 1;
    char *r = malloc(n);
    snprintf(r, n, "%s%s", a, b);
    return r;
}

/* ── attribute-read raise models ──
 * SimpleNamespace doubles (self/source/claim/failure/restored): py3.14
 * module-qualified message. AttrDouble-shaped doubles (home/cfg/transport/
 * adapter/store): the double's own "missing attribute" message. */
static json_t *rn3_attr_ns(const json_t *node, const char *key) {
    /* returns a COPY of the value, or a raise spec when absent */
    const json_t *v = rn3_oget(node, key);
    if (v) return json_copy(v);
    char buf[192];
    snprintf(buf, sizeof(buf), "'types.SimpleNamespace' object has no attribute '%s'", key);
    return rn3_raise("AttributeError", buf);
}
static json_t *rn3_attr_double(const json_t *node, const char *key) {
    const json_t *v = rn3_oget(node, key);
    if (v) return json_copy(v);
    char buf[160];
    snprintf(buf, sizeof(buf), "missing attribute '%s'", key);
    return rn3_raise("AttributeError", buf);
}
static json_t *rn3_none_attr(const char *key) {
    char buf[160];
    snprintf(buf, sizeof(buf), "'NoneType' object has no attribute '%s'", key);
    return rn3_raise("AttributeError", buf);
}
/* receiver.method attribute resolution for hook doubles: null receiver ->
 * NoneType message; object without the hook -> missing-attribute; non-object
 * -> the type-named message. Returns NULL when the hook is present. */
static json_t *rn3_hook_check(const json_t *recv, const char *hook) {
    if (rn3_is_null(recv)) return rn3_none_attr(hook);
    if (recv->type == JSON_OBJECT) {
        if (json_has(recv, hook)) return NULL;
        char buf[160];
        snprintf(buf, sizeof(buf), "missing attribute '%s'", hook);
        return rn3_raise("AttributeError", buf);
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", rn3_typename(recv), hook);
    return rn3_raise("AttributeError", buf);
}

/* ── logging seams (RecLogger twin) ──
 * logger.<level>(fmt, *args, exc_info=?): {fmt, args?, exc_info?};
 * logger.log(level, fmt, *args, exc_info=?): {fmt, level, args?, exc_info?}.
 * Empty positional args omit "args"; exc_info only when true. A BaseException
 * positional arg projects to its {"__raise__","msg"} spec (str(exc) form). */
static json_t *rn3_log_named(const rn3_seams_t *s, const char *name, const char *fmt,
                             json_t *args_arr, bool exc_info) {
    json_t *a = rn3_args();
    rn3_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    if (exc_info) rn3_b(a, "exc_info", true);
    return rn3_call(s, name, a);
}
static json_t *rn3_log(const rn3_seams_t *s, const char *level, const char *fmt,
                       json_t *args_arr, bool exc_info) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return rn3_log_named(s, name, fmt, args_arr, exc_info);
}

/* ── set helpers (arrays of tuples, deep-equality membership) ── */
static bool rn3_set_has(const json_t *set, const json_t *item) {
    if (!rn3_is_arr(set)) return false;
    for (size_t i = 0; i < json_len(set); i++)
        if (rn3_eq(json_get(set, i), item)) return true;
    return false;
}
static void rn3_set_add(json_t *set, const json_t *item) {
    if (!rn3_set_has(set, item)) json_append(set, json_copy(item));
}
static void rn3_set_discard(json_t *set, const json_t *item) {
    if (!rn3_is_arr(set) || !item) return;
    for (size_t i = 0; i < json_len(set); i++) {
        if (rn3_eq(json_get(set, i), item)) {
            json_free(set->c.items[i]);
            for (size_t k = i; k + 1 < set->c.count; k++) set->c.items[k] = set->c.items[k + 1];
            set->c.count--;
            return;
        }
    }
}
static void rn3_set_union(json_t *dst, const json_t *src) {
    if (!rn3_is_arr(src)) return;
    for (size_t i = 0; i < json_len(src); i++) rn3_set_add(dst, json_get(src, i));
}
static bool rn3_set_subset(const json_t *a, const json_t *b) {
    if (!rn3_is_arr(a)) return true;
    for (size_t i = 0; i < json_len(a); i++)
        if (!rn3_set_has(b, json_get(a, i))) return false;
    return true;
}
/* sort key: compact JSON serialization (matches the oracle's
 * json.dumps(separators=(",",":")) key over the ASCII fixture domain). */
static int rn3_set_cmp(const void *pa, const void *pb) {
    const json_t *a = *(json_t *const *)pa, *b = *(json_t *const *)pb;
    char *sa = json_serialize(a), *sb = json_serialize(b);
    int c = strcmp(sa ? sa : "", sb ? sb : "");
    free(sa); free(sb);
    return c;
}
static json_t *rn3_set_sorted(const json_t *set) {
    json_t *r = set ? json_copy(set) : json_array();
    if (r->type == JSON_ARRAY && r->c.count > 1)
        qsort(r->c.items, r->c.count, sizeof(json_t *), rn3_set_cmp);
    return r;
}

/* ── PurePosixPath component join over the documented root domain ── */
static char *rn3_path_join(const char *root, const char *comp) {
    /* normalize: drop "." components and trailing slashes; keep a single
     * leading "/" for absolute roots; ""/"." -> the bare component. */
    if (!root) root = "";
    bool absolute = root[0] == '/';
    size_t cap = strlen(root) + strlen(comp) + 2;
    char *r = malloc(cap);
    size_t len = 0;
    if (absolute) r[len++] = '/';
    const char *p = root;
    bool any = false;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != '/') e++;
        size_t n = (size_t)(e - p);
        if (!(n == 1 && p[0] == '.')) {
            if (any) r[len++] = '/';
            memcpy(r + len, p, n);
            len += n;
            any = true;
        }
        p = e;
    }
    if (any) r[len++] = '/';
    memcpy(r + len, comp, strlen(comp));
    len += strlen(comp);
    r[len] = '\0';
    return r;
}

/* ── json.loads over the canonical-text domain ──
 * Empty / whitespace-only / invalid-start single-line text raises the live
 * JSONDecodeError "Expecting value: line 1 column N (char M)". Other invalid
 * shapes are outside the fixture domain (canonical json.dumps text only). */
static json_t *rn3_json_loads(const char *text) {
    if (!text) text = "";
    size_t i = 0;
    while (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r') i++;
    char c = text[i];
    bool start_ok = c == '{' || c == '[' || c == '"' || c == '-' ||
                    (c >= '0' && c <= '9') || c == 't' || c == 'f' || c == 'n';
    if (!c || !start_ok) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Expecting value: line 1 column %zu (char %zu)", i + 1, i);
        return rn3_raise("JSONDecodeError", buf);
    }
    char *err = NULL;
    json_t *v = json_parse(text, &err);
    if (err || !v) {
        free(err);
        json_free(v);
        return rn3_raise("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)");
    }
    return v;
}
/* strip ONE leading UTF-8 BOM (utf-8-sig read_text). */
static char *rn3_strip_bom(const char *s) {
    if (!s) return strdup("");
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        return strdup(s + 3);
    return strdup(s);
}

/* ── scope markers / CM enter-exit ── */
static bool rn3_scope_is_nullcontext(const json_t *marker) {
    const json_t *k = rn3_oget(marker, "scope");
    const char *s = rn3_strval(k);
    return s && strcmp(s, "nullcontext") == 0;
}
static json_t *rn3_scope_enter(const rn3_seams_t *s, const json_t *marker) {
    if (rn3_scope_is_nullcontext(marker)) return NULL;
    json_t *a = rn3_args(); rn3_j(a, "scope", marker);
    return rn3_call(s, "scope_enter", a);
}
static json_t *rn3_scope_exit(const rn3_seams_t *s, const json_t *marker) {
    if (rn3_scope_is_nullcontext(marker)) return NULL;
    json_t *a = rn3_args(); rn3_j(a, "scope", marker);
    return rn3_call(s, "scope_exit", a);
}

/* ── real _send_failed / _send_error (gateway/run_shutdown.py) ── */
static bool rn3_send_failed(const json_t *result) {
    if (rn3_is_null(result)) return false;
    const json_t *succ = rn3_oget(result, "success");
    /* getattr(result, "success", True) is False — JSON_BOOL-false identity */
    return succ && succ->type == JSON_BOOL && !succ->bool_val;
}
static json_t *rn3_send_error(const json_t *result) {
    const json_t *err = rn3_oget(result, "error");
    if (err) return json_copy(err);
    return json_string("send returned success=False");
}

/* self projection for seams taking the runner: the C self node IS the
 * projected state (the oracle drops callable attrs and denorms doubles).
 * Set-valued state (_completion_deliveries_inflight) projects SORTED, matching
 * the oracle's denorm(set) -> sorted list. */
static json_t *rn3_proj_self(const json_t *self) {
    json_t *copy = self ? json_copy(self) : json_object();
    /* class-attribute default: GatewayNotificationsMixin._planned_restart_notice_lock
     * = None is present on every real runner; model it in the projection. */
    if (!json_has(copy, "_planned_restart_notice_lock"))
        json_set(copy, "_planned_restart_notice_lock", json_null());
    json_t *inflight = json_obj_get(copy, "_completion_deliveries_inflight");
    if (inflight && inflight->type == JSON_ARRAY && inflight->c.count > 1)
        qsort(inflight->c.items, inflight->c.count, sizeof(json_t *), rn3_set_cmp);
    return copy;
}

/* platform value projection for seam-result platforms: the script carries the
 * enum .value STRING directly (the oracle materializes it into a Platform
 * double); an object with "value" is also accepted. */
static char *rn3_platform_value(const json_t *platform) {
    const char *s = rn3_strval(platform);
    if (s) return strdup(s);
    return rn3_pystr(rn3_oget(platform, "value"));
}

/* ── exported flow cores (forward declarations) ── */
static json_t *rn3_send_queued_final_text(const json_t *self, const json_t *adapter,
                                          const json_t *source, const char *text_content,
                                          const json_t *metadata, const json_t *event_message_id,
                                          const json_t *session_key, const json_t *inbound_message_id,
                                          const rn3_seams_t *s);
static json_t *rn3_send_home_channel_message(const json_t *self, const char *platform_value,
                                             const json_t *home, const json_t *transport,
                                             const char *message, const char *failure_fmt,
                                             const rn3_seams_t *s);
static json_t *rn3_replay_pending_planned_restart_notification(json_t *self, const rn3_seams_t *s);
static json_t *rn3_send_session_db_warning_notifications(const json_t *self, const rn3_seams_t *s);
static json_t *rn3_self_post_api_server(const json_t *self, const json_t *adapter,
                                        const char *synth_text, const char *raw_sid,
                                        const json_t *evt, const rn3_seams_t *s);
static json_t *rn3_committed_relay_adapter(const json_t *self, const json_t *source,
                                           const rn3_seams_t *s);
static json_t *rn3_completion_event_scope(const json_t *self, const json_t *evt,
                                          const rn3_seams_t *s);
static json_t *rn3_served_api_server_event_scope_enter(const json_t *self, const json_t *evt,
                                                       const char *raw_sid, const rn3_seams_t *s,
                                                       json_t **scope_out);
static json_t *rn3_served_api_server_event_scope_exit(const json_t *scope, const rn3_seams_t *s);
static json_t *rn3_deliver_completion_notification_scoped(json_t *self, const char *synth_text,
                                                          const json_t *evt, const json_t *sibling_claims,
                                                          const rn3_seams_t *s);
static json_t *rn3_settle_batch_waiters(json_t *entries, const json_t *result, const rn3_seams_t *s);
static json_t *rn3_deliver_async_delegation_group(const json_t *self, const json_t *group,
                                                  const rn3_seams_t *s);
static json_t *rn3_deliver_async_delegation_groups(const json_t *self, const json_t *groups,
                                                   const json_t *pr, const rn3_seams_t *s);
static json_t *rn3_arm_process_watcher(const json_t *self, const json_t *watcher,
                                       const rn3_seams_t *s);

/* ── 1. _send_queued_final_text: ledger-bracketed queued-lane final ── */
/* PoP: rn3_send_queued_final_text @ gateway/run_notifications.py:_send_queued_final_text */
static json_t *rn3_send_queued_final_text(const json_t *self, const json_t *adapter,
                                          const json_t *source, const char *text_content,
                                          const json_t *metadata, const json_t *event_message_id,
                                          const json_t *session_key, const json_t *inbound_message_id,
                                          const rn3_seams_t *s) {
    (void)self;
    json_t *result = NULL;
    bool ledgered = rn3_truth(session_key) && rn3_truth(rn3_oget(adapter, "__base__"));
    if (ledgered) {
        /* LOAD_ATTR adapter.send_final_ledgered BEFORE the argument seams */
        json_t *hc = rn3_hook_check(adapter, "send_final_ledgered");
        if (hc) return hc;
        /* MessageEvent(text="", source=source, ledger_message_id=inbound_message_id) */
        json_t *event = json_object();
        rn3_s(event, "text", "");
        rn3_j(event, "source", source);
        rn3_j(event, "ledger_message_id", inbound_message_id);
        /* _mark_notify_metadata(metadata) fires before the send seam */
        json_t *a = rn3_args(); rn3_j(a, "metadata", metadata);
        json_t *meta = rn3_call(s, "_mark_notify_metadata", a);
        if (rn3_raised(meta)) { json_free(event); return meta; }
        a = rn3_args();
        json_set(a, "event", event);
        rn3_j(a, "session_key", session_key);
        rn3_s(a, "text", text_content);
        json_set(a, "metadata", meta);
        rn3_j(a, "reply_to", event_message_id);
        json_t *sr = rn3_call(s, "adapter.send_final_ledgered", a);
        if (rn3_raised(sr)) return sr;
        /* result, _ = <2-tuple> */
        json_t *uc = rn3_unpack_check(sr, 2);
        if (uc) { json_free(sr); return uc; }
        result = json_copy(json_get(sr, 0));
        json_free(sr);
    } else {
        /* LOAD_ATTR adapter.send BEFORE source.chat_id argument evaluation */
        json_t *hc = rn3_hook_check(adapter, "send");
        if (hc) return hc;
        json_t *chat = rn3_attr_ns(source, "chat_id");
        if (rn3_raised(chat)) return chat;
        json_t *a = rn3_args();
        json_set(a, "chat_id", chat);
        rn3_s(a, "message", text_content);
        rn3_j(a, "metadata", metadata);
        result = rn3_call(s, "adapter.send", a);
        if (rn3_raised(result)) return result;
    }
    /* if not getattr(result, "success", False): logger.warning(...) */
    if (!rn3_truth(rn3_oget(result, "success"))) {
        const json_t *chat = rn3_oget(source, "chat_id");
        const json_t *err = rn3_oget(result, "error");
        json_t *la = json_array();
        json_append(la, chat ? json_copy(chat) : json_string("?"));
        if (rn3_truth(err)) json_append(la, json_copy(err));
        else json_append(la, json_string("no result"));
        json_t *lr = rn3_log(s, "warning", "Queued-lane final send to %s failed: %s", la, false);
        if (rn3_raised(lr)) { json_free(result); return lr; }
        json_free(lr);
    }
    return result;
}

/* ── 2. _send_home_channel_message: best-effort home-channel send ── */
/* PoP: rn3_send_home_channel_message @ gateway/run_notifications.py:_send_home_channel_message */
static json_t *rn3_send_home_channel_message(const json_t *self, const char *platform_value,
                                             const json_t *home, const json_t *transport,
                                             const char *message, const char *failure_fmt,
                                             const rn3_seams_t *s) {
    (void)self;
    json_t *body_raise = NULL;
    json_t *ret = NULL;
    /* metadata = self._thread_metadata_for_target(platform, home.chat_id,
     *              home.thread_id, adapter=transport.adapter) */
    json_t *chat_id = rn3_attr_double(home, "chat_id");
    if (rn3_raised(chat_id)) { body_raise = chat_id; goto handled; }
    json_t *thread_id = rn3_attr_double(home, "thread_id");
    if (rn3_raised(thread_id)) { json_free(chat_id); body_raise = thread_id; goto handled; }
    json_t *tadapter = rn3_attr_double(transport, "adapter");
    if (rn3_raised(tadapter)) { json_free(chat_id); json_free(thread_id); body_raise = tadapter; goto handled; }
    json_t *a = rn3_args();
    rn3_s(a, "platform", platform_value);
    json_set(a, "chat_id", chat_id);
    json_set(a, "thread_id", thread_id);
    json_set(a, "adapter", tadapter);
    json_t *metadata = rn3_call(s, "self._thread_metadata_for_target", a);
    if (rn3_raised(metadata)) { body_raise = metadata; goto handled; }
    /* if transport.is_relay: metadata = dict(metadata or {}); user_id/scope_id */
    json_t *is_relay = rn3_attr_double(transport, "is_relay");
    if (rn3_raised(is_relay)) { json_free(metadata); body_raise = is_relay; goto handled; }
    bool relay = rn3_truth(is_relay);
    json_free(is_relay);
    if (relay) {
        json_t *md = rn3_truth(metadata) ? json_copy(metadata) : json_object();
        if (!rn3_is_obj(md)) {
            /* dict(non-dict truthy) is outside the fixture domain */
            json_free(md); json_free(metadata);
            body_raise = rn3_raise("TypeError", "object is not iterable");
            goto handled;
        }
        json_free(metadata);
        metadata = md;
        json_t *uid = rn3_attr_double(home, "user_id");
        if (rn3_raised(uid)) { json_free(metadata); body_raise = uid; goto handled; }
        if (rn3_truth(uid)) json_set(metadata, "user_id", json_copy(uid));
        json_free(uid);
        json_t *scope = rn3_attr_double(home, "scope_id");
        if (rn3_raised(scope)) { json_free(metadata); body_raise = scope; goto handled; }
        if (rn3_truth(scope)) json_set(metadata, "scope_id", json_copy(scope));
        json_free(scope);
    }
    /* send_metadata = _non_conversational_metadata(metadata, platform=platform) */
    a = rn3_args();
    rn3_j(a, "metadata", metadata);
    rn3_s(a, "platform", platform_value);
    json_t *send_metadata = rn3_call(s, "_non_conversational_metadata", a);
    if (rn3_raised(send_metadata)) { json_free(metadata); body_raise = send_metadata; goto handled; }
    /* if send_metadata is not None or transport.is_relay: transport.send(...)
     * else: transport.adapter.send(str(home.chat_id), message) */
    json_t *is_relay2 = rn3_attr_double(transport, "is_relay");
    if (rn3_raised(is_relay2)) { json_free(metadata); json_free(send_metadata); body_raise = is_relay2; goto handled; }
    bool relay2 = rn3_truth(is_relay2);
    json_free(is_relay2);
    json_t *result = NULL;
    if (!rn3_is_null(send_metadata) || relay2) {
        json_t *hc = rn3_hook_check(transport, "send");
        if (hc) { json_free(metadata); json_free(send_metadata); body_raise = hc; goto handled; }
        json_t *chat2 = rn3_attr_double(home, "chat_id");
        if (rn3_raised(chat2)) { json_free(metadata); json_free(send_metadata); body_raise = chat2; goto handled; }
        char *chat_s = rn3_pystr(chat2);
        json_free(chat2);
        a = rn3_args();
        rn3_s(a, "platform", platform_value);
        rn3_s(a, "chat_id", chat_s);
        free(chat_s);
        rn3_s(a, "message", message);
        json_set(a, "metadata", send_metadata);
        result = rn3_call(s, "transport.send", a);
        json_free(metadata);
        if (rn3_raised(result)) { body_raise = result; goto handled; }
    } else {
        json_free(send_metadata);
        json_t *tadapter2 = rn3_attr_double(transport, "adapter");
        if (rn3_raised(tadapter2)) { json_free(metadata); body_raise = tadapter2; goto handled; }
        json_t *hc = rn3_hook_check(tadapter2, "send");
        if (hc) { json_free(metadata); json_free(tadapter2); body_raise = hc; goto handled; }
        json_t *chat2 = rn3_attr_double(home, "chat_id");
        if (rn3_raised(chat2)) { json_free(metadata); json_free(tadapter2); body_raise = chat2; goto handled; }
        char *chat_s = rn3_pystr(chat2);
        json_free(chat2);
        a = rn3_args();
        rn3_s(a, "chat_id", chat_s);
        free(chat_s);
        rn3_s(a, "message", message);
        rn3_j(a, "metadata", NULL);
        result = rn3_call(s, "adapter.send", a);
        json_free(metadata);
        json_free(tadapter2);
        if (rn3_raised(result)) { body_raise = result; goto handled; }
    }
    /* if _send_failed(result): logger.warning(failure_fmt, platform.value,
     *                             home.chat_id, _send_error(result)); return False */
    if (rn3_send_failed(result)) {
        json_t *chat3 = rn3_attr_double(home, "chat_id");
        if (rn3_raised(chat3)) { json_free(result); body_raise = chat3; goto handled; }
        json_t *la = json_array();
        json_append(la, json_string(platform_value));
        json_append(la, chat3);
        json_append(la, rn3_send_error(result));
        json_t *lr = rn3_log(s, "warning", failure_fmt, la, false);
        json_free(result);
        if (rn3_raised(lr)) { body_raise = lr; goto handled; }
        json_free(lr);
        ret = json_bool(false);
    } else {
        json_free(result);
        ret = json_bool(true);
    }
handled:
    if (body_raise) {
        if (rn3_is_base_exc(rn3_kind(body_raise))) return body_raise;
        /* except Exception as exc: logger.warning(failure_fmt, platform.value,
         *                            home.chat_id, exc); return False */
        json_t *chat_h = rn3_attr_double(home, "chat_id");
        if (rn3_raised(chat_h)) { json_free(body_raise); return chat_h; }
        json_t *la = json_array();
        json_append(la, json_string(platform_value));
        json_append(la, chat_h);
        json_append(la, rn3_exc_value(body_raise));
        json_t *lr = rn3_log(s, "warning", failure_fmt, la, false);
        json_free(body_raise);
        if (rn3_raised(lr)) return lr;
        json_free(lr);
        return json_bool(false);
    }
    return ret;
}

/* ── 3. _replay_pending_planned_restart_notification: marker replay ── */
/* PoP: rn3_replay_pending_planned_restart_notification @ gateway/run_notifications.py:_replay_pending_planned_restart_notification */
static json_t *rn3_replay_pending_planned_restart_notification(json_t *self, const rn3_seams_t *s) {
    /* self._note_restart_notice_boot() — OUTSIDE the try */
    json_t *r = rn3_call(s, "self._note_restart_notice_boot", rn3_args());
    if (rn3_raised(r)) return r;
    json_free(r);
    /* if self._planned_restart_notice_lock is None: create (state, no seam) */
    const json_t *lock = rn3_oget(self, "_planned_restart_notice_lock");
    if (rn3_is_null(lock)) json_set(self, "_planned_restart_notice_lock", json_bool(true));
    /* async with lock: single-threaded model, no events */
    json_t *path = rn3_call(s, "_planned_restart_notification_path", rn3_args());
    if (rn3_raised(path)) return path;
    char *path_s = rn3_pystr(path);
    json_free(path);
    json_t *pa = rn3_args(); rn3_s(pa, "path", path_s);
    json_t *ex = rn3_call(s, "path.exists", pa);
    if (rn3_raised(ex)) { free(path_s); return ex; }
    bool exists = rn3_truth(ex);
    json_free(ex);
    if (!exists) { free(path_s); return NULL; }
    /* try: data = json.loads(path.read_text(encoding="utf-8-sig")) */
    json_t *body_raise = NULL;
    json_t *data = NULL;
    json_t *delivered = json_array();
    json_t *owed = json_array();
    json_t *ta = rn3_args(); rn3_s(ta, "path", path_s); rn3_s(ta, "encoding", "utf-8-sig");
    json_t *txt = rn3_call(s, "path.read_text", ta);
    if (rn3_raised(txt)) { body_raise = txt; goto replay_done; }
    char *raw = rn3_pystr(txt);
    json_free(txt);
    char *text = rn3_strip_bom(raw);
    free(raw);
    data = rn3_json_loads(text);
    free(text);
    if (rn3_raised(data)) { body_raise = data; data = NULL; goto replay_done; }
    /* delivered = {tuple(target) for target in data.get("delivered_targets", [])} */
    if (!rn3_is_obj(data)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'", rn3_typename(data));
        body_raise = rn3_raise("AttributeError", buf);
        goto replay_done;
    }
    const json_t *dt = rn3_oget(data, "delivered_targets");
    if (dt && dt->type != JSON_NULL) {
        if (!rn3_is_arr(dt)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "'%s' object is not iterable", rn3_typename(dt));
            body_raise = rn3_raise("TypeError", buf);
            goto replay_done;
        }
        for (size_t i = 0; i < json_len(dt); i++) {
            const json_t *target = json_get(dt, i);
            if (!rn3_is_arr(target)) {
                char buf[128];
                snprintf(buf, sizeof(buf), "'%s' object is not iterable", rn3_typename(target));
                body_raise = rn3_raise("TypeError", buf);
                goto replay_done;
            }
            rn3_set_add(delivered, target);
        }
    }
    /* owed = {_served_notice_target_key(...) for ... in self._served_home_channel_configs()
     *         if cfg.home_channel and cfg.home_channel.chat_id and cfg.gateway_restart_notification} */
    json_t *configs = rn3_call(s, "self._served_home_channel_configs", rn3_args());
    if (rn3_raised(configs)) { body_raise = configs; goto replay_done; }
    if (!rn3_is_arr(configs)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable", rn3_typename(configs));
        json_free(configs);
        body_raise = rn3_raise("TypeError", buf);
        goto replay_done;
    }
    for (size_t i = 0; i < json_len(configs); i++) {
        const json_t *triple = json_get(configs, i);
        json_t *uc = rn3_unpack_check(triple, 3);
        if (uc) { json_free(configs); body_raise = uc; goto replay_done; }
        const json_t *profile = json_get(triple, 0);
        const json_t *platform = json_get(triple, 1);
        const json_t *cfg = json_get(triple, 2);
        json_t *hc = rn3_attr_double(cfg, "home_channel");
        if (rn3_raised(hc)) { json_free(configs); body_raise = hc; goto replay_done; }
        bool hc_truth = rn3_truth(hc);
        json_t *chat = NULL, *restart = NULL;
        if (hc_truth) {
            chat = rn3_attr_double(hc, "chat_id");
            if (rn3_raised(chat)) { json_free(hc); json_free(configs); body_raise = chat; goto replay_done; }
            if (rn3_truth(chat)) {
                restart = rn3_attr_double(cfg, "gateway_restart_notification");
                if (rn3_raised(restart)) { json_free(hc); json_free(chat); json_free(configs); body_raise = restart; goto replay_done; }
            }
        }
        bool pass = hc_truth && rn3_truth(chat) && rn3_truth(restart);
        json_free(hc); json_free(chat); json_free(restart);
        if (!pass) continue;
        const json_t *hc2 = rn3_oget(cfg, "home_channel");
        json_t *chat2 = rn3_attr_double(hc2, "chat_id");
        json_t *thread2 = rn3_attr_double(hc2, "thread_id");
        if (rn3_raised(chat2)) { json_free(thread2); json_free(configs); body_raise = chat2; goto replay_done; }
        if (rn3_raised(thread2)) { json_free(chat2); json_free(configs); body_raise = thread2; goto replay_done; }
        char *pv = rn3_platform_value(platform);
        json_t *a = rn3_args();
        rn3_j(a, "profile", profile);
        rn3_s(a, "platform_value", pv);
        free(pv);
        json_set(a, "chat_id", chat2);
        json_set(a, "thread_id", thread2);
        json_t *key = rn3_call(s, "_served_notice_target_key", a);
        if (rn3_raised(key)) { json_free(configs); body_raise = key; goto replay_done; }
        json_t *kuc = rn3_unpack_check(key, 3);
        if (kuc) { json_free(key); json_free(configs); body_raise = kuc; goto replay_done; }
        rn3_set_add(owed, key);
        json_free(key);
    }
    json_free(configs);
    /* delivered |= await self._send_home_channel_startup_notifications(skip_targets=delivered) */
    {
        json_t *sorted = rn3_set_sorted(delivered);
        json_t *a = rn3_args();
        json_set(a, "skip_targets", sorted);
        json_t *more = rn3_call(s, "self._send_home_channel_startup_notifications", a);
        if (rn3_raised(more)) { body_raise = more; goto replay_done; }
        if (more && more->type != JSON_NULL) {
            if (!rn3_is_arr(more)) {
                char buf[128];
                snprintf(buf, sizeof(buf), "'%s' object is not iterable", rn3_typename(more));
                json_free(more);
                body_raise = rn3_raise("TypeError", buf);
                goto replay_done;
            }
            for (size_t i = 0; i < json_len(more); i++) {
                const json_t *t = json_get(more, i);
                if (!rn3_is_arr(t)) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "'%s' object is not iterable", rn3_typename(t));
                    json_free(more);
                    body_raise = rn3_raise("TypeError", buf);
                    goto replay_done;
                }
                rn3_set_add(delivered, t);
            }
        }
        json_free(more);
    }
    /* if owed <= delivered: path.unlink(missing_ok=True); return */
    if (rn3_set_subset(owed, delivered)) {
        json_t *a = rn3_args();
        rn3_s(a, "path", path_s);
        rn3_b(a, "missing_ok", true);
        json_t *ur = rn3_call(s, "path.unlink", a);
        if (rn3_raised(ur)) { body_raise = ur; goto replay_done; }
        json_free(ur);
        goto replay_done;   /* return None (success) */
    }
    /* data["delivered_targets"] = [list(target) for target in delivered] (SORTED) */
    json_set(data, "delivered_targets", rn3_set_sorted(delivered));
    {
        json_t *a = rn3_args();
        rn3_s(a, "path", path_s);
        rn3_j(a, "data", data);
        rn3_j(a, "indent", NULL);
        json_t *wr = rn3_call(s, "atomic_json_write", a);
        if (rn3_raised(wr)) { body_raise = wr; goto replay_done; }
        json_free(wr);
    }
replay_done:
    free(path_s);
    json_free(data);
    json_free(delivered);
    json_free(owed);
    if (body_raise) {
        if (rn3_is_base_exc(rn3_kind(body_raise))) return body_raise;
        /* except Exception: logger.warning("Planned-restart notification remains pending",
         *                                  exc_info=True) */
        json_t *lr = rn3_log(s, "warning", "Planned-restart notification remains pending",
                             json_array(), true);
        json_free(body_raise);
        if (rn3_raised(lr)) return lr;
        json_free(lr);
    }
    return NULL;
}

/* ── 4. _send_session_db_warning_notifications: state.db warning broadcast ── */
/* the real present_notification (gateway/warning_notifications.py) transcription:
 * diagnostic defaults True; warning_notifications_enabled gates the callback. */
static json_t *rn3_present_notification(const json_t *self, const char *platform_value,
                                        const json_t *home, const json_t *transport,
                                        const char *message, const rn3_seams_t *s) {
    json_t *a = rn3_args();
    rn3_s(a, "platform", platform_value);
    rn3_j(a, "user_config", NULL);
    json_t *en = rn3_call(s, "warning_notifications_enabled", a);
    if (rn3_raised(en)) return en;
    bool enabled = rn3_truth(en);
    json_free(en);
    if (!enabled) return json_bool(false);   /* diagnostic=True gate; result discarded */
    /* await present() — the lambda calls the ported _send_home_channel_message */
    json_t *r = rn3_send_home_channel_message(self, platform_value, home, transport, message,
                                              "state.db warning notification failed for %s:%s: %s", s);
    if (rn3_raised(r)) return r;
    json_free(r);
    return json_bool(true);
}
/* PoP: rn3_send_session_db_warning_notifications @ gateway/run_notifications.py:_send_session_db_warning_notifications */
static json_t *rn3_send_session_db_warning_notifications(const json_t *self, const rn3_seams_t *s) {
    /* error = getattr(self, "_session_db_init_error", None); if not error: return */
    const json_t *error0 = rn3_oget(self, "_session_db_init_error");
    if (!rn3_truth(error0)) return NULL;
    json_t *error = NULL;   /* owned copy */
    /* if getattr(self, "_session_db_handle_cache", None) is not None: re-check */
    const json_t *cache = rn3_oget(self, "_session_db_handle_cache");
    if (!rn3_is_null(cache)) {
        json_t *r = rn3_call(s, "self._open_session_db_for_active_scope", rn3_args());
        if (rn3_raised(r)) return r;
        json_free(r);
        /* error = self._session_db_init_error — DIRECT attribute read */
        json_t *err2 = rn3_attr_ns(self, "_session_db_init_error");
        if (rn3_raised(err2)) return err2;
        bool still = rn3_truth(err2);
        if (!still) {
            json_free(err2);
            json_t *lr = rn3_log(s, "info",
                "state.db recovered before the home-channel warning went out; not broadcasting",
                json_array(), false);
            if (rn3_raised(lr)) return lr;
            json_free(lr);
            return NULL;
        }
        error = err2;
    } else {
        error = json_copy(error0);
    }
    /* cause = classify_persistence_error(error); profile_arg = profile_cli_selector() */
    json_t *a = rn3_args(); rn3_j(a, "error", error);
    json_t *cause = rn3_call(s, "classify_persistence_error", a);
    if (rn3_raised(cause)) { json_free(error); return cause; }
    json_t *parg = rn3_call(s, "profile_cli_selector", rn3_args());
    if (rn3_raised(parg)) { json_free(cause); json_free(error); return parg; }
    const char *cause_s = rn3_strval(cause);
    json_t *message = NULL;
    if (cause_s && strcmp(cause_s, "corrupt") == 0) {
        json_t *scope = rn3_call(s, "profile_scope", rn3_args());
        if (rn3_raised(scope)) { json_free(cause); json_free(parg); json_free(error); return scope; }
        a = rn3_args(); rn3_j(a, "scope", scope);
        json_t *db_path = rn3_call(s, "store_location", a);
        json_free(scope);
        if (rn3_raised(db_path)) { json_free(cause); json_free(parg); json_free(error); return db_path; }
        json_t *root = rn3_call(s, "get_default_hermes_root", rn3_args());
        if (rn3_raised(root)) { json_free(db_path); json_free(cause); json_free(parg); json_free(error); return root; }
        char *root_s = rn3_pystr(root);
        json_free(root);
        char *backups = rn3_path_join(root_s, "backups");
        free(root_s);
        a = rn3_args();
        rn3_s(a, "key", "gateway.startup.db_corrupt");
        rn3_j(a, "profile_arg", parg);
        rn3_j(a, "db_path", db_path);
        rn3_s(a, "backups_dir", backups);
        free(backups);
        json_free(db_path);
        message = rn3_call(s, "t", a);
    } else if (cause_s && strcmp(cause_s, "fts_index") == 0) {
        a = rn3_args();
        rn3_s(a, "key", "gateway.startup.db_fts_corrupt");
        rn3_j(a, "profile_arg", parg);
        message = rn3_call(s, "t", a);
    } else {
        a = rn3_args(); rn3_j(a, "error", error);
        json_t *failure = rn3_call(s, "describe_storage_failure", a);
        if (rn3_raised(failure)) { json_free(cause); json_free(parg); json_free(error); return failure; }
        /* action = failure.action; failure.cause / failure.gloss are direct reads */
        json_t *action = rn3_attr_ns(failure, "action");
        if (rn3_raised(action)) { json_free(failure); json_free(cause); json_free(parg); json_free(error); return action; }
        json_t *fcause = rn3_attr_ns(failure, "cause");
        if (rn3_raised(fcause)) { json_free(action); json_free(failure); json_free(cause); json_free(parg); json_free(error); return fcause; }
        /* failure.cause not in _SELF_CLEARING_STORAGE_CAUSES (frozenset) */
        bool self_clearing = false;
        const char *fc = rn3_strval(fcause);
        if (fc) {
            self_clearing = strcmp(fc, "compression") == 0 ||
                            strcmp(fc, "compression_closed") == 0 ||
                            strcmp(fc, "turn_lease") == 0;
        } else if (!rn3_is_null(fcause)) {
            /* unhashable set element (py3.14 wording) */
            char buf[160];
            snprintf(buf, sizeof(buf), "cannot use '%s' as a set element (unhashable type: '%s')",
                     rn3_typename(fcause), rn3_typename(fcause));
            json_free(fcause); json_free(action); json_free(failure);
            json_free(cause); json_free(parg); json_free(error);
            return rn3_raise("TypeError", buf);
        }
        json_free(fcause);
        if (!self_clearing) {
            a = rn3_args();
            rn3_s(a, "key", "gateway.startup.db_then_restart");
            json_set(a, "action", action);
            rn3_j(a, "profile_arg", parg);
            json_t *wrapped = rn3_call(s, "t", a);
            if (rn3_raised(wrapped)) { json_free(failure); json_free(cause); json_free(parg); json_free(error); return wrapped; }
            action = wrapped;
        }
        json_t *gloss = rn3_attr_ns(failure, "gloss");
        if (rn3_raised(gloss)) { json_free(action); json_free(failure); json_free(cause); json_free(parg); json_free(error); return gloss; }
        a = rn3_args();
        rn3_s(a, "key", "gateway.startup.db_unavailable");
        json_set(a, "cause", gloss);
        json_set(a, "action", action);
        json_free(failure);
        message = rn3_call(s, "t", a);
    }
    json_free(cause);
    json_free(parg);
    if (rn3_raised(message)) { json_free(error); return message; }
    /* logger.warning("Broadcasting state.db failure warning to home channels: %s", error) */
    json_t *la = json_array();
    json_append(la, json_copy(error));
    json_free(error);
    json_t *lr = rn3_log(s, "warning", "Broadcasting state.db failure warning to home channels: %s", la, false);
    if (rn3_raised(lr)) { json_free(message); return lr; }
    json_free(lr);
    /* for platform, _cfg, home, transport in self._home_channel_transports():
     *     await present_notification(lambda: self._send_home_channel_message(...), platform=platform) */
    json_t *transports = rn3_call(s, "self._home_channel_transports", rn3_args());
    if (rn3_raised(transports)) { json_free(message); return transports; }
    if (!rn3_is_arr(transports)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable", rn3_typename(transports));
        json_free(transports); json_free(message);
        return rn3_raise("TypeError", buf);
    }
    char *msg_s = rn3_pystr(message);
    json_free(message);
    for (size_t i = 0; i < json_len(transports); i++) {
        const json_t *quad = json_get(transports, i);
        json_t *uc = rn3_unpack_check(quad, 4);
        if (uc) { free(msg_s); json_free(transports); return uc; }
        const json_t *platform = json_get(quad, 0);
        const json_t *home = json_get(quad, 2);
        const json_t *transport = json_get(quad, 3);
        char *pv = rn3_platform_value(platform);
        json_t *pr = rn3_present_notification(self, pv, home, transport, msg_s, s);
        free(pv);
        if (rn3_raised(pr)) { free(msg_s); json_free(transports); return pr; }
        json_free(pr);
    }
    free(msg_s);
    json_free(transports);
    return NULL;
}

/* ── 5. _self_post_api_server: api_server wake / durable persist ── */
/* PoP: rn3_self_post_api_server @ gateway/run_notifications.py:_self_post_api_server */
static json_t *rn3_self_post_api_server(const json_t *self, const json_t *adapter,
                                        const char *synth_text, const char *raw_sid,
                                        const json_t *evt, const rn3_seams_t *s) {
    (void)self;
    const char *tys = rn3_strval(rn3_oget(evt, "type"));
    bool is_ad = tys && strcmp(tys, "async_delegation") == 0;
    const char *info = is_ad
        ? "Async delegation completion \xe2\x80\x94 persisting delivery row for api_server session %s (no wake turn)"
        : "Watch pattern notification \xe2\x80\x94 waking api_server session %s via self-post";
    const char *fail = is_ad
        ? "Async delegation delivery persist failed for session %s: %s"
        : "Watch notification self-post wake failed for session %s: %s";
    json_t *scope = json_object();
    rn3_s(scope, "scope", "nullcontext");
    json_t *served = NULL;
    if (!is_ad) {
        /* try: served = await asyncio.to_thread(self._served_api_server_wake_profile, evt, raw_sid)
         * except LookupError: logger.warning(fail, raw_sid, e); return False */
        json_t *a = rn3_args(); rn3_j(a, "evt", evt); rn3_s(a, "raw_sid", raw_sid);
        served = rn3_call(s, "self._served_api_server_wake_profile", a);
        if (rn3_raised(served)) {
            if (rn3_exc_is(rn3_kind(served), "LookupError")) {
                json_t *la = json_array();
                json_append(la, json_string(raw_sid));
                json_append(la, rn3_exc_value(served));
                json_t *lr = rn3_log(s, "warning", fail, la, false);
                json_free(served);
                json_free(scope);
                if (rn3_raised(lr)) return lr;
                json_free(lr);
                return json_bool(false);
            }
            json_free(scope);
            return served;   /* other kinds propagate (no catch here) */
        }
        if (rn3_truth(served)) {
            /* source = SessionSource(platform=Platform.API_SERVER, chat_id=raw_sid, profile=served) */
            json_t *source = json_object();
            rn3_s(source, "platform", "api_server");
            rn3_s(source, "chat_id", raw_sid);
            rn3_j(source, "profile", served);
            a = rn3_args(); rn3_j(a, "source", source);
            json_t *home = rn3_call(s, "self._resolve_profile_home_for_source", a);
            json_free(source);
            if (rn3_raised(home)) { json_free(served); json_free(scope); return home; }
            char *home_s = rn3_pystr(home);
            json_free(home);
            a = rn3_args(); rn3_s(a, "home", home_s);
            free(home_s);
            json_t *sc = rn3_call(s, "_async_profile_runtime_scope", a);
            if (rn3_raised(sc)) { json_free(served); json_free(scope); return sc; }
            json_free(scope);
            scope = sc;
        }
    }
    /* try: logger.info(info, raw_sid); async with scope: await deliver(); return True
     * except Exception as e: logger.warning(fail, raw_sid, e); return False */
    json_t *body_raise = NULL;
    json_t *la = json_array();
    json_append(la, json_string(raw_sid));
    json_t *lr = rn3_log(s, "info", info, la, false);
    if (rn3_raised(lr)) { body_raise = lr; goto sp_done; }
    json_free(lr);
    {
        json_t *er = rn3_scope_enter(s, scope);
        if (rn3_raised(er)) { body_raise = er; goto sp_done; }
        json_free(er);
        /* await deliver() */
        json_t *dr = NULL;
        if (is_ad) {
            json_t *a = rn3_args();
            rn3_j(a, "adapter", adapter);
            rn3_s(a, "text", synth_text);
            rn3_s(a, "session_id", raw_sid);
            rn3_j(a, "evt", evt);
            dr = rn3_call(s, "persist_delegation_delivery", a);
        } else {
            /* text=_mark_internal_notification(synth_text) fires FIRST, then
             * notification_category's diagnostic_process_event(evt) */
            json_t *a = rn3_args(); rn3_s(a, "text", synth_text);
            json_t *marked = rn3_call(s, "_mark_internal_notification", a);
            if (rn3_raised(marked)) { dr = marked; }
            else {
                a = rn3_args(); rn3_j(a, "evt", evt);
                json_t *diag = rn3_call(s, "diagnostic_process_event", a);
                if (rn3_raised(diag)) { json_free(marked); dr = diag; }
                else {
                    bool d = rn3_truth(diag);
                    json_free(diag);
                    a = rn3_args();
                    rn3_j(a, "adapter", adapter);
                    json_set(a, "text", marked);
                    rn3_s(a, "session_id", raw_sid);
                    rn3_j(a, "profile", served);
                    rn3_s(a, "notification_category", d ? "diagnostic" : "result");
                    dr = rn3_call(s, "deliver_wake", a);
                }
            }
        }
        json_t *deliver_raise = rn3_raised(dr) ? dr : NULL;
        if (!deliver_raise) json_free(dr);
        /* __exit__ runs even when the body raised; an exit raise overrides */
        json_t *ex = rn3_scope_exit(s, scope);
        if (rn3_raised(ex)) {
            if (deliver_raise) json_free(deliver_raise);
            body_raise = ex;
        } else {
            json_free(ex);
            if (deliver_raise) body_raise = deliver_raise;
        }
    }
sp_done:
    json_free(scope);
    json_free(served);
    if (body_raise) {
        if (rn3_is_base_exc(rn3_kind(body_raise))) return body_raise;
        json_t *la2 = json_array();
        json_append(la2, json_string(raw_sid));
        json_append(la2, rn3_exc_value(body_raise));
        json_t *lr2 = rn3_log(s, "warning", fail, la2, false);
        json_free(body_raise);
        if (rn3_raised(lr2)) return lr2;
        json_free(lr2);
        return json_bool(false);
    }
    return json_bool(true);
}

/* ── 6. _committed_relay_adapter: committed-admission relay proof ── */
/* PoP: rn3_committed_relay_adapter @ gateway/run_notifications.py:_committed_relay_adapter */
static json_t *rn3_committed_relay_adapter(const json_t *self, const json_t *source,
                                           const rn3_seams_t *s) {
    json_t *a = rn3_args();
    json_set(a, "self", rn3_proj_self(self));
    json_t *authority = rn3_call(s, "active_authority", a);
    if (rn3_raised(authority)) return authority;
    if (rn3_is_null(authority)) { json_free(authority); return NULL; }
    /* entry = self.session_store.lookup_by_session_key(
     *             self.session_store._generate_session_key(source)) */
    json_t *ss = rn3_attr_ns(self, "session_store");
    if (rn3_raised(ss)) { json_free(authority); return ss; }
    a = rn3_args(); rn3_j(a, "source", source);
    json_t *sk = rn3_call(s, "session_store._generate_session_key", a);
    if (rn3_raised(sk)) { json_free(ss); json_free(authority); return sk; }
    a = rn3_args(); rn3_j(a, "session_key", sk);
    json_free(sk);
    json_t *entry = rn3_call(s, "session_store.lookup_by_session_key", a);
    json_free(ss);
    if (rn3_raised(entry)) { json_free(authority); return entry; }
    /* envelope = committed_route_envelope(authority, entry) if entry is not None else None */
    json_t *envelope = NULL;
    if (!rn3_is_null(entry)) {
        a = rn3_args();
        rn3_j(a, "authority", authority);
        rn3_j(a, "entry", entry);
        envelope = rn3_call(s, "committed_route_envelope", a);
        if (rn3_raised(envelope)) { json_free(entry); json_free(authority); return envelope; }
    }
    json_free(entry);
    json_free(authority);
    /* if envelope is None or 'relay' not in envelope['provenance']: return None */
    if (rn3_is_null(envelope)) { json_free(envelope); return NULL; }
    const json_t *prov = rn3_oget(envelope, "provenance");
    if (!prov) {
        json_free(envelope);
        return rn3_raise("KeyError", "'provenance'");
    }
    bool has_relay = false;
    if (prov->type == JSON_OBJECT) {
        has_relay = json_has(prov, "relay");
    } else if (prov->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(prov); i++) {
            const json_t *el = json_get(prov, i);
            const char *es = rn3_strval(el);
            if (es && strcmp(es, "relay") == 0) { has_relay = true; break; }
        }
    } else if (prov->type == JSON_STRING) {
        has_relay = strstr(prov->str_val ? prov->str_val : "", "relay") != NULL;
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "argument of type '%s' is not iterable", rn3_typename(prov));
        json_free(envelope);
        return rn3_raise("TypeError", buf);
    }
    if (!has_relay) { json_free(envelope); return NULL; }
    /* restored = restore_native({'text': '', 'native_text_v1':
     *      {k: v for k, v in envelope.items() if k != 'media'}}, self) */
    json_t *payload = json_object();
    rn3_s(payload, "text", "");
    json_t *native = json_object();
    for (size_t i = 0; i < envelope->c.count; i++) {
        if (strcmp(envelope->c.keys[i], "media") == 0) continue;
        json_set(native, envelope->c.keys[i], json_copy(envelope->c.items[i]));
    }
    json_set(payload, "native_text_v1", native);
    a = rn3_args();
    json_set(a, "envelope", payload);
    json_set(a, "self", rn3_proj_self(self));
    json_t *restored = rn3_call(s, "restore_native", a);
    json_free(envelope);
    if (rn3_raised(restored)) {
        if (rn3_exc_is(rn3_kind(restored), "AuthorityRefusal")) { json_free(restored); return NULL; }
        return restored;
    }
    /* return self._adapter_for_source(restored.source) */
    json_t *rsource = rn3_attr_ns(restored, "source");
    json_free(restored);
    if (rn3_raised(rsource)) return rsource;
    a = rn3_args();
    json_set(a, "source", rsource);
    json_t *ad = rn3_call(s, "self._adapter_for_source", a);
    return ad;
}

/* ── 7. _completion_event_scope: profile runtime scope of a completion ── */
/* PoP: rn3_completion_event_scope @ gateway/run_notifications.py:_completion_event_scope */
static json_t *rn3_completion_event_scope(const json_t *self, const json_t *evt,
                                          const rn3_seams_t *s) {
    json_t *a = rn3_args(); rn3_j(a, "evt", evt);
    json_t *source = rn3_call(s, "self._build_process_event_source", a);
    if (rn3_raised(source)) return source;
    const json_t *profile = rn3_oget(source, "profile");
    if (rn3_is_null(source) || !rn3_truth(profile)) {
        json_free(source);
        a = rn3_args(); rn3_j(a, "evt", evt);
        json_t *raw = rn3_call(s, "_raw_process_event_session_id", a);
        if (rn3_raised(raw)) return raw;
        bool has_raw = rn3_truth(raw);
        const json_t *config = rn3_oget(self, "config");
        bool multiplex = rn3_truth(rn3_oget(config, "multiplex_profiles"));
        if (has_raw && multiplex) {
            /* return self._served_api_server_event_scope(evt, raw_sid) — CM
             * CREATION only: the body runs on enter, no seams fire here.
             * raw_sid is the SAME _raw_process_event_session_id result. */
            char *raw_s = rn3_pystr(raw);
            json_free(raw);
            json_t *marker = json_object();
            rn3_s(marker, "scope", "served_api_server");
            rn3_j(marker, "evt", evt);
            rn3_s(marker, "raw_sid", raw_s);
            free(raw_s);
            return marker;
        }
        json_free(raw);
        return rn3_call(s, "async_launch_profile_scope_if_multiplexed", rn3_args());
    }
    /* profile_home = self._resolve_profile_home_for_source(source) */
    a = rn3_args(); rn3_j(a, "source", source);
    json_free(source);
    json_t *home = rn3_call(s, "self._resolve_profile_home_for_source", a);
    if (rn3_raised(home)) return home;
    char *home_s = rn3_pystr(home);
    json_t *override = rn3_call(s, "get_hermes_home_override", rn3_args());
    if (rn3_raised(override)) { free(home_s); json_free(home); return override; }
    /* get_hermes_home_override() == str(profile_home) */
    json_t *home_str = json_string(home_s);
    bool same = rn3_eq(override, home_str);
    json_free(home_str);
    json_free(override);
    if (same) {
        free(home_s);
        json_free(home);
        json_t *marker = json_object();
        rn3_s(marker, "scope", "nullcontext");
        return marker;
    }
    a = rn3_args(); rn3_s(a, "home", home_s);
    free(home_s);
    json_free(home);
    return rn3_call(s, "_async_profile_runtime_scope", a);
}

/* ── 8. _served_api_server_event_scope: asynccontextmanager body ──
 * enter fires the body seams up to scope creation + scope_enter; exit fires
 * scope_exit. The CM marker itself is {"scope":"served_api_server",...}. */
/* PoP: rn3_served_api_server_event_scope @ gateway/run_notifications.py:_served_api_server_event_scope */
static json_t *rn3_served_api_server_event_scope_enter(const json_t *self, const json_t *evt,
                                                       const char *raw_sid, const rn3_seams_t *s,
                                                       json_t **scope_out) {
    (void)self;
    *scope_out = NULL;
    json_t *a = rn3_args(); rn3_j(a, "evt", evt); rn3_s(a, "raw_sid", raw_sid);
    json_t *served = rn3_call(s, "self._served_api_server_wake_profile", a);
    if (rn3_raised(served)) return served;
    json_t *scope = NULL;
    if (rn3_truth(served)) {
        json_t *source = json_object();
        rn3_s(source, "platform", "api_server");
        rn3_s(source, "chat_id", raw_sid);
        rn3_j(source, "profile", served);
        a = rn3_args(); rn3_j(a, "source", source);
        json_t *home = rn3_call(s, "self._resolve_profile_home_for_source", a);
        json_free(source);
        if (rn3_raised(home)) { json_free(served); return home; }
        char *home_s = rn3_pystr(home);
        json_free(home);
        a = rn3_args(); rn3_s(a, "home", home_s);
        free(home_s);
        scope = rn3_call(s, "_async_profile_runtime_scope", a);
        if (rn3_raised(scope)) { json_free(served); return scope; }
    } else {
        scope = rn3_call(s, "async_launch_profile_scope_if_multiplexed", rn3_args());
        if (rn3_raised(scope)) { json_free(served); return scope; }
    }
    json_free(served);
    json_t *er = rn3_scope_enter(s, scope);
    if (rn3_raised(er)) { json_free(scope); return er; }
    json_free(er);
    *scope_out = scope;
    return NULL;
}
static json_t *rn3_served_api_server_event_scope_exit(const json_t *scope, const rn3_seams_t *s) {
    json_t *ex = rn3_scope_exit(s, scope);
    if (rn3_raised(ex)) return ex;
    json_free(ex);
    return NULL;
}

/* ── 9. _deliver_completion_notification_scoped: claim/refund/settle ── */
/* settle one durable claim row (self._settle_durable_claim seam). */
static json_t *rn3_settle_one(const rn3_seams_t *s, const char *operation,
                              const char *delegation_id, const char *claim_id) {
    json_t *a = rn3_args();
    rn3_s(a, "kind", operation);
    rn3_s(a, "delegation_id", delegation_id);
    rn3_s(a, "claim_id", claim_id);
    return rn3_call(s, "self._settle_durable_claim", a);
}
/* PoP: rn3_deliver_completion_notification_scoped @ gateway/run_notifications.py:_deliver_completion_notification_scoped */
static json_t *rn3_deliver_completion_notification_scoped(json_t *self, const char *synth_text,
                                                          const json_t *evt_in, const json_t *sibling_claims,
                                                          const rn3_seams_t *s) {
    /* if not should_surface_notification(evt): return None (OUTSIDE try) */
    json_t *a = rn3_args(); rn3_j(a, "evt", evt_in);
    json_t *surface = rn3_call(s, "should_surface_notification", a);
    if (rn3_raised(surface)) return surface;
    bool surf = rn3_truth(surface);
    json_free(surface);
    if (!surf) return NULL;
    /* restore_completion_route(self, str(evt.get("session_key") or "")) */
    char *sk = rn3_str_or_empty(rn3_oget(evt_in, "session_key"));
    a = rn3_args();
    json_set(a, "self", rn3_proj_self(self));
    rn3_s(a, "session_key", sk);
    free(sk);
    json_t *rr = rn3_call(s, "restore_completion_route", a);
    if (rn3_raised(rr)) return rr;
    json_free(rr);
    /* evt = await follow_detached_pin(self, evt) */
    a = rn3_args();
    json_set(a, "self", rn3_proj_self(self));
    rn3_j(a, "evt", evt_in);
    json_t *evt = rn3_call(s, "follow_detached_pin", a);
    if (rn3_raised(evt)) return evt;
    /* identity = self._completion_delivery_identity(evt) */
    a = rn3_args(); rn3_j(a, "evt", evt);
    json_t *identity = rn3_call(s, "self._completion_delivery_identity", a);
    if (rn3_raised(identity)) { json_free(evt); return identity; }
    bool has_identity = !rn3_is_null(identity);
    /* claim = self._CompletionClaim() — defaults */
    json_t *claim = json_object();
    rn3_s(claim, "delegation_id", "");
    rn3_s(claim, "claim_id", "");
    rn3_b(claim, "proceed", true);
    rn3_j(claim, "early_result", NULL);
    rn3_s(claim, "owner_session_id", "");
    rn3_s(claim, "proven_session_id", "");
    rn3_b(claim, "defer", false);
    bool accepted = false, identity_claimed = false, refused = false;
    json_t *body_raise = NULL;
    json_t *ret = NULL;
    bool ret_set = false;
    /* try: claim = await self._preflight_completion_delivery(evt) */
    a = rn3_args(); rn3_j(a, "evt", evt);
    json_t *pf = rn3_call(s, "self._preflight_completion_delivery", a);
    if (rn3_raised(pf)) { body_raise = pf; goto sc_finally; }
    json_free(claim);
    claim = pf;
    /* if not claim.proceed: return claim.early_result */
    json_t *proceed = rn3_attr_ns(claim, "proceed");
    if (rn3_raised(proceed)) { body_raise = proceed; goto sc_finally; }
    bool proceed_b = rn3_truth(proceed);
    json_free(proceed);
    if (!proceed_b) {
        json_t *early = rn3_attr_ns(claim, "early_result");
        if (rn3_raised(early)) { body_raise = early; goto sc_finally; }
        ret = early; ret_set = true;
        goto sc_finally;
    }
    /* if getattr(self, 'session_authority', None) is not None:
     *     if completion_admission(self, evt) is not None: accepted = True; return True */
    if (!rn3_is_null(rn3_oget(self, "session_authority"))) {
        a = rn3_args();
        json_set(a, "self", rn3_proj_self(self));
        rn3_j(a, "evt", evt);
        json_t *adm = rn3_call(s, "completion_admission", a);
        if (rn3_raised(adm)) { body_raise = adm; goto sc_finally; }
        bool admitted = !rn3_is_null(adm);
        json_free(adm);
        if (admitted) {
            accepted = true;
            ret = json_bool(true); ret_set = true;
            goto sc_finally;
        }
    }
    /* if identity is not None:
     *     if self._completion_identity_seen(identity, claim=True): return None
     *     identity_claimed = True */
    if (has_identity) {
        a = rn3_args();
        rn3_j(a, "identity", identity);
        rn3_b(a, "claim", true);
        json_t *seen = rn3_call(s, "self._completion_identity_seen", a);
        if (rn3_raised(seen)) { body_raise = seen; goto sc_finally; }
        bool seen_b = rn3_truth(seen);
        json_free(seen);
        if (seen_b) { ret = NULL; ret_set = true; goto sc_finally; }
        identity_claimed = true;
    }
    /* if claim.owner_session_id: evt = {**evt, parent/proven} */
    if (rn3_truth(rn3_oget(claim, "owner_session_id"))) {
        json_t *copy = json_copy(evt);
        json_set(copy, "parent_session_id", json_copy(rn3_oget(claim, "owner_session_id")));
        json_set(copy, "proven_session_id", json_copy(rn3_oget(claim, "proven_session_id")));
        json_free(evt);
        evt = copy;
    }
    /* completion_ids = [claim.delegation_id] if claim.claim_id else []
     * completion_ids += [sibling["delegation_id"] for sibling, claim_id in sibling_claims if claim_id] */
    json_t *completion_ids = json_array();
    if (rn3_truth(rn3_oget(claim, "claim_id")))
        json_append(completion_ids, json_copy(rn3_oget(claim, "delegation_id")));
    if (rn3_is_arr(sibling_claims)) {
        for (size_t i = 0; i < json_len(sibling_claims); i++) {
            const json_t *pair = json_get(sibling_claims, i);
            json_t *uc = rn3_unpack_check(pair, 2);
            if (uc) { json_free(completion_ids); body_raise = uc; goto sc_finally; }
            const json_t *sibling = json_get(pair, 0);
            const json_t *cid = json_get(pair, 1);
            if (!rn3_truth(cid)) continue;
            const json_t *did = rn3_oget(sibling, "delegation_id");
            if (!did) {
                json_free(completion_ids);
                body_raise = rn3_raise("KeyError", "'delegation_id'");
                goto sc_finally;
            }
            json_append(completion_ids, json_copy(did));
        }
    }
    /* evt = {**evt, "completion_ids": completion_ids} */
    {
        json_t *copy = json_copy(evt);
        json_set(copy, "completion_ids", completion_ids);
        json_free(evt);
        evt = copy;
    }
    /* injection_result = await self._inject_watch_notification(synth_text, evt, raise_not_accepted=True) */
    a = rn3_args();
    rn3_s(a, "synth_text", synth_text);
    rn3_j(a, "evt", evt);
    rn3_b(a, "raise_not_accepted", true);
    json_t *inj = rn3_call(s, "self._inject_watch_notification", a);
    if (rn3_raised(inj)) {
        if (rn3_exc_is(rn3_kind(inj), "WakeNotAccepted")) {
            /* refused = not exc.permanent; return False */
            const json_t *perm = rn3_oget(inj, "permanent");
            refused = !rn3_truth(perm);
            json_free(inj);
            ret = json_bool(false); ret_set = true;
            goto sc_finally;
        }
        body_raise = inj;   /* other kinds propagate through finally */
        goto sc_finally;
    }
    /* if injection_result is not True: return injection_result (identity) */
    if (!(inj && inj->type == JSON_BOOL && inj->bool_val)) {
        ret = inj; ret_set = true;
        goto sc_finally;
    }
    json_free(inj);
    accepted = true;
    /* if identity is not None: with lock: self._mark_completions_delivered_locked((identity,)) */
    if (has_identity) {
        a = rn3_args();
        json_t *ids = json_array();
        json_append(ids, json_copy(identity));
        json_set(a, "identities", ids);
        json_t *mr = rn3_call(s, "self._mark_completions_delivered_locked", a);
        if (rn3_raised(mr)) { body_raise = mr; goto sc_finally; }
        json_free(mr);
    }
    ret = json_bool(true); ret_set = true;
sc_finally:
    /* finally: runs on EVERY exit path; a finally raise OVERRIDES the body
     * raise / early return. */
    {
        json_t *fin_raise = NULL;
        /* if identity_claimed and not accepted: inflight.discard(identity) */
        if (identity_claimed && !accepted) {
            json_t *inflight = (self && self->type == JSON_OBJECT)
                ? json_obj_get(self, "_completion_deliveries_inflight") : NULL;
            if (inflight && inflight->type == JSON_ARRAY) rn3_set_discard(inflight, identity);
        }
        /* operation = "complete" if accepted else "defer" if refused or claim.defer else "release" */
        const char *operation = accepted ? "complete"
            : (refused || rn3_truth(rn3_oget(claim, "defer"))) ? "defer" : "release";
        const char *claim_id = rn3_strval(rn3_oget(claim, "claim_id"));
        const char *delegation_id = rn3_strval(rn3_oget(claim, "delegation_id"));
        if (claim_id && claim_id[0]) {
            json_t *sr = rn3_settle_one(s, operation, delegation_id ? delegation_id : "", claim_id);
            if (rn3_raised(sr)) fin_raise = sr;
            else json_free(sr);
        }
        if (!fin_raise && rn3_is_arr(sibling_claims)) {
            for (size_t i = 0; i < json_len(sibling_claims); i++) {
                const json_t *pair = json_get(sibling_claims, i);
                json_t *uc = rn3_unpack_check(pair, 2);
                if (uc) { fin_raise = uc; break; }
                const json_t *sibling = json_get(pair, 0);
                const json_t *cid = json_get(pair, 1);
                if (!rn3_truth(cid)) continue;
                const json_t *did = rn3_oget(sibling, "delegation_id");
                if (!did) { fin_raise = rn3_raise("KeyError", "'delegation_id'"); break; }
                char *did_s = rn3_pystr(did);
                char *cid_s = rn3_pystr(cid);
                json_t *sr = rn3_settle_one(s, operation, did_s, cid_s);
                free(did_s); free(cid_s);
                if (rn3_raised(sr)) { fin_raise = sr; break; }
                json_free(sr);
            }
        }
        /* if accepted and sibling_claims: self._record_coalesced_completion_siblings([...]) */
        if (!fin_raise && accepted && rn3_truth(sibling_claims)) {
            json_t *events = json_array();
            bool ok = true;
            if (rn3_is_arr(sibling_claims)) {
                for (size_t i = 0; i < json_len(sibling_claims); i++) {
                    const json_t *pair = json_get(sibling_claims, i);
                    json_t *uc = rn3_unpack_check(pair, 2);
                    if (uc) { fin_raise = uc; ok = false; break; }
                    json_append(events, json_copy(json_get(pair, 0)));
                }
            }
            if (ok) {
                a = rn3_args();
                json_set(a, "events", events);
                json_t *rc = rn3_call(s, "self._record_coalesced_completion_siblings", a);
                if (rn3_raised(rc)) fin_raise = rc;
                else json_free(rc);
            } else {
                json_free(events);
            }
        }
        if (fin_raise) {
            if (ret_set) json_free(ret);
            if (body_raise) json_free(body_raise);
            json_free(claim);
            json_free(identity);
            json_free(evt);
            return fin_raise;
        }
    }
    json_free(claim);
    json_free(identity);
    json_free(evt);
    if (body_raise) return body_raise;
    if (ret_set) return ret;
    return NULL;
}

/* ── 10. _settle_batch_waiters: resolve watcher futures ── */
/* PoP: rn3_settle_batch_waiters @ gateway/run_notifications.py:_settle_batch_waiters */
static json_t *rn3_settle_batch_waiters(json_t *entries, const json_t *result, const rn3_seams_t *s) {
    if (!rn3_is_arr(entries)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable", rn3_typename(entries));
        return rn3_raise("TypeError", buf);
    }
    for (size_t i = 0; i < json_len(entries); i++) {
        const json_t *triple = json_get(entries, i);
        json_t *uc = rn3_unpack_check(triple, 3);
        if (uc) return uc;
        json_t *future = json_get(triple, 2);   /* borrowed; mutated in place below */
        /* if not future.done(): future.set_result(result) */
        const json_t *fid = rn3_oget(future, "id");
        char *fid_s = rn3_pystr(fid);
        json_t *a = rn3_args(); rn3_s(a, "id", fid_s);
        json_t *done = rn3_call(s, "future.done", a);
        if (rn3_raised(done)) { free(fid_s); return done; }
        bool done_b = rn3_truth(done);
        json_free(done);
        if (!done_b) {
            a = rn3_args();
            rn3_s(a, "id", fid_s);
            rn3_j(a, "result", result);
            json_t *sr = rn3_call(s, "future.set_result", a);
            if (rn3_raised(sr)) { free(fid_s); return sr; }
            json_free(sr);
            /* live set_result mutates the future: done=True, result=result */
            if (future && future->type == JSON_OBJECT) {
                json_set(future, "done", json_bool(true));
                json_set(future, "result", result ? json_copy(result) : json_null());
            }
        }
        free(fid_s);
    }
    return NULL;
}

/* ── 11. _deliver_async_delegation_group: scoped group wrapper ── */
/* PoP: rn3_deliver_async_delegation_group @ gateway/run_notifications.py:_deliver_async_delegation_group */
static json_t *rn3_deliver_async_delegation_group(const json_t *self, const json_t *group,
                                                  const rn3_seams_t *s) {
    /* group[0] — IndexError on empty (OUTSIDE any try) */
    json_t *first = rn3_index0(group);
    if (rn3_raised(first)) return first;
    json_t *a = rn3_args();
    json_set(a, "self", rn3_proj_self(self));
    json_set(a, "evt", first);
    rn3_s(a, "label", "async delegation completions");
    json_t *scope = rn3_call(s, "completion_event_scope_or_refuse", a);
    if (rn3_raised(scope)) return scope;
    if (rn3_is_null(scope)) { json_free(scope); return NULL; }
    /* async with scope: return await self._deliver_async_delegation_group_scoped(group) */
    json_t *er = rn3_scope_enter(s, scope);
    if (rn3_raised(er)) { json_free(scope); return er; }
    json_free(er);
    a = rn3_args(); rn3_j(a, "group", group);
    json_t *r = rn3_call(s, "self._deliver_async_delegation_group_scoped", a);
    json_t *body_raise = rn3_raised(r) ? r : NULL;
    json_t *ret = body_raise ? NULL : r;
    json_t *ex = rn3_scope_exit(s, scope);
    json_free(scope);
    if (rn3_raised(ex)) {
        if (body_raise) json_free(body_raise);
        if (ret) json_free(ret);
        return ex;
    }
    json_free(ex);
    if (body_raise) return body_raise;
    return ret;
}

/* ── 12. _deliver_async_delegation_groups: per-group delivery + requeue ── */
static json_t *rn3_queue_put(const rn3_seams_t *s, const json_t *pr, const json_t *evt) {
    const json_t *q = rn3_oget(pr, "completion_queue");
    const json_t *qid = rn3_oget(q, "id");
    char *qid_s = rn3_pystr(qid);
    json_t *a = rn3_args();
    rn3_s(a, "queue", qid_s);
    free(qid_s);
    rn3_j(a, "evt", evt);
    return rn3_call(s, "queue.put", a);
}
/* PoP: rn3_deliver_async_delegation_groups @ gateway/run_notifications.py:_deliver_async_delegation_groups */
static json_t *rn3_deliver_async_delegation_groups(const json_t *self, const json_t *groups,
                                                   const json_t *pr, const rn3_seams_t *s) {
    if (!rn3_is_obj(groups)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object has no attribute 'values'", rn3_typename(groups));
        return rn3_raise("AttributeError", buf);
    }
    for (size_t gi = 0; gi < groups->c.count; gi++) {
        const json_t *group = groups->c.items[gi];
        /* if any(evt.get("_watch_next_retry_at", 0) > time.monotonic() for evt in group):
         *     requeue all; continue  (OUTSIDE the try) */
        bool hold = false;
        json_t *gate_raise = NULL;
        if (rn3_is_arr(group)) {
            for (size_t i = 0; i < json_len(group); i++) {
                const json_t *evt = json_get(group, i);
                const json_t *nra = rn3_oget(evt, "_watch_next_retry_at");
                json_t *lhs = nra ? json_copy(nra) : json_number(0.0);
                json_t *now = rn3_call(s, "time.monotonic", rn3_args());
                if (rn3_raised(now)) { json_free(lhs); gate_raise = now; break; }
                json_t *cmp = rn3_gt(lhs, now);
                json_free(lhs); json_free(now);
                if (rn3_raised(cmp)) { gate_raise = cmp; break; }
                bool gt = rn3_truth(cmp);
                json_free(cmp);
                if (gt) { hold = true; break; }
            }
        }
        if (gate_raise) return gate_raise;
        if (hold) {
            if (rn3_is_arr(group)) {
                for (size_t i = 0; i < json_len(group); i++) {
                    json_t *qr = rn3_queue_put(s, pr, json_get(group, i));
                    if (rn3_raised(qr)) return qr;
                    json_free(qr);
                }
            }
            continue;
        }
        /* try: delivered = await self._deliver_async_delegation_group(group)
         *      if delivered is False: back_off + requeue
         * except Exception as e: requeue + logger.error(..., exc_info=True) */
        json_t *delivered = rn3_deliver_async_delegation_group(self, group, s);
        json_t *body_raise = NULL;
        if (rn3_raised(delivered)) {
            body_raise = delivered;
        } else {
            bool is_false = delivered && delivered->type == JSON_BOOL && !delivered->bool_val;
            json_free(delivered);
            if (is_false) {
                json_t *a = rn3_args(); rn3_j(a, "group", group);
                json_t *bo = rn3_call(s, "self._back_off_refused_completions", a);
                if (rn3_raised(bo)) { body_raise = bo; }
                else {
                    json_free(bo);
                    if (rn3_is_arr(group)) {
                        for (size_t i = 0; i < json_len(group); i++) {
                            json_t *qr = rn3_queue_put(s, pr, json_get(group, i));
                            if (rn3_raised(qr)) { body_raise = qr; break; }
                            json_free(qr);
                        }
                    }
                }
            }
        }
        if (body_raise) {
            if (rn3_is_base_exc(rn3_kind(body_raise))) return body_raise;
            /* except Exception: requeue every evt, then logger.error */
            if (rn3_is_arr(group)) {
                for (size_t i = 0; i < json_len(group); i++) {
                    json_t *qr = rn3_queue_put(s, pr, json_get(group, i));
                    if (rn3_raised(qr)) { json_free(body_raise); return qr; }
                    json_free(qr);
                }
            }
            json_t *la = json_array();
            json_append(la, rn3_exc_value(body_raise));
            json_t *lr = rn3_log(s, "error", "Async delegation injection error: %s", la, true);
            json_free(body_raise);
            if (rn3_raised(lr)) return lr;
            json_free(lr);
        }
    }
    return NULL;
}

/* ── 13. arm_process_watcher: live watcher arming from the tool thread ── */
/* PoP: rn3_arm_process_watcher @ gateway/run_notifications.py:arm_process_watcher */
static json_t *rn3_arm_process_watcher(const json_t *self, const json_t *watcher,
                                       const rn3_seams_t *s) {
    const json_t *loop = rn3_oget(self, "_gateway_loop");
    if (!rn3_truth(rn3_oget(self, "_running")) || rn3_is_null(loop)) return json_bool(false);
    /* loop.is_running() — a class method on the loop double (always present);
     * a non-object loop raises the type-named AttributeError. */
    if (loop->type != JSON_OBJECT) {
        char buf[160];
        snprintf(buf, sizeof(buf), "'%s' object has no attribute 'is_running'", rn3_typename(loop));
        return rn3_raise("AttributeError", buf);
    }
    const json_t *lid = rn3_oget(loop, "id");
    char *lid_s = rn3_pystr(lid);
    json_t *a = rn3_args(); rn3_s(a, "id", lid_s);
    free(lid_s);
    json_t *running = rn3_call(s, "loop.is_running", a);
    if (rn3_raised(running)) return running;
    bool is_running = rn3_truth(running);
    json_free(running);
    if (!is_running) return json_bool(false);
    /* future = contextvars.Context().run(safe_schedule_threadsafe,
     *     self._run_process_watcher(watcher), loop, logger=logger, log_message=...)
     * Context().run is transparent; the coro seam fires first (arg order). */
    a = rn3_args(); rn3_j(a, "watcher", watcher);
    json_t *coro = rn3_call(s, "self._run_process_watcher", a);
    if (rn3_raised(coro)) return coro;
    a = rn3_args();
    json_set(a, "coro", coro);
    rn3_j(a, "loop", loop);
    rn3_s(a, "logger", "logger");
    rn3_s(a, "log_message", "Live process watcher arming failed");
    json_t *future = rn3_call(s, "safe_schedule_threadsafe", a);
    if (rn3_raised(future)) return future;
    bool ok = !rn3_is_null(future);
    json_free(future);
    return json_bool(ok);
}
