/* relay_shared_metrics_flow2.c — FLOW + leaf port of the UNCLAIMED remainder of
 * hermes_cli/observability/relay_shared_metrics.py (the claimed siblings live in
 * relay_shared_metrics.c / relay_shared_metrics_flow.c / _gaps and are SEAMED
 * here through the injected universal seam rsm2_io_t).
 *
 * Decision-sequence port: the pure event/identity/clock leaves, the lazy config
 * readers, the guarded-emission / scope-stack cores, session/task resolution,
 * the abort/start-user-turn lifecycle, the deregister/send-thread/release
 * teardown, the lineage-spent read, and the module-level runtime-gate wrappers.
 * ALL collaborator I/O (claimed siblings, foreign-module imports, the Relay
 * host/subscriber/thread/clock, logger, the _RUNTIMES registry gates) rides the
 * universal seam; the harness scripts results and records the ordered trace the
 * oracle diffs against LIVE relay_shared_metrics.py under the same scripts.
 * Ported functions call each other REAL (internal composition:
 * _resolved_send_config -> _raw_config, _emit_rows -> _guarded,
 * _release -> _join_send_thread, and the _text leaf everywhere).
 *
 * Representation contract (shared with tests/oracle_relay_shared_metrics_flow2.py):
 *   - raise specs are VALUES {"__raise__": kind, "message": msg} threaded through
 *     every helper; an uncaught one becomes the case ret. BaseException trio
 *     (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError) passes
 *     through every `except Exception` guard exactly as live.
 *   - fail-closed: an absent / exhausted seam script returns a RuntimeError raise
 *     spec ("no script for <name>"), recorded as the event ret BEFORE propagation.
 *   - object doubles (session/task/turn/runtime/thread/lineage) are attribute
 *     dicts read via rsm2_get; tuple/set -> array (sets sorted); None -> null;
 *     integral floats collapse to ints on both sides.
 *   - identity (`is`, id()-dedup in _sole / _forget) is modelled by a "ref"
 *     marker string on the doubles; equal refs == the same live object.
 *   - callable doubles {"__callable__": "<seam>"} fire that seam with
 *     {"args": [...], "kwargs": {...}} when invoked.
 *   - scripts: a bare JSON array is a per-call QUEUE (one result per call); a
 *     list RESULT must be wrapped {"__script__": true, "ret": [...]}. A scripted
 *     null returns None. None-valued seam-arg OBJECT keys dropped on both sides.
 *   - fixture domain (ASCII scope): str()/strip()/lower() operands are ASCII;
 *     _text/_sole/_forget operands are scalars or ref-marked doubles; object
 *     seam results are null or a NON-EMPTY record; attribute doubles carry the
 *     fields the live body reads (cost, lease, relay_session, relay_enabled...).
 *     EXCLUDED: integral-FLOAT literals in str()/_text positions (libjson has no
 *     int/float identity, so 7.0 reads as 7 and str() would diverge from live
 *     "7.0"); use integral-ints or non-integral floats there.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/relay_shared_metrics_flow2.c". rsm_ / rsm_flow_ prefixes are
 * taken by flow1; this port uses rsm2_.
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
typedef struct rsm2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} rsm2_io_t;

/* ── truthiness / accessors (Python-faithful; no libjson string coercion) ── */
static bool rsm2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool rsm2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rsm2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool rsm2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool rsm2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool rsm2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool rsm2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}

static const char *rsm2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *rsm2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* numeric read: ONLY a JSON_NUMBER counts (Python int/float); else default. */
static double rsm2_num(const json_t *o, const char *k, double def)
{
    const json_t *v = rsm2_get(o, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static bool rsm2_bool(const json_t *o, const char *k, bool def)
{
    const json_t *v = rsm2_get(o, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : def;
}

static bool rsm2_str_eq(const char *a, const char *b)
{
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static json_t *rsm2_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* identity marker (`is` / id()): equal ref strings == the same live object. */
static const char *rsm2_ref(const json_t *v)
{
    const char *r = rsm2_str(rsm2_get(v, "ref"));
    return r ? r : "";
}

/* ── raise specs ── */
static bool rsm2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *rsm2_kind(const json_t *r)
{
    const char *k = rsm2_str(rsm2_get(r, "__raise__"));
    return k ? k : "";
}
static bool rsm2_is_base(const char *kind)
{
    return rsm2_str_eq(kind, "SystemExit") || rsm2_str_eq(kind, "KeyboardInterrupt")
        || rsm2_str_eq(kind, "GeneratorExit") || rsm2_str_eq(kind, "CancelledError");
}
static json_t *rsm2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *rsm2_xfmt(const char *fmt, ...)
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

/* ── universal seam call: args OWNED by caller, freed here; result caller-owned
 * (NULL == None, or a raise spec). Fail-closed when no channel installed. ── */
static json_t *rsm2_call(rsm2_io_t *io, const char *name, json_t *args)
{
    if (!io || !io->call) {
        char *m = rsm2_xfmt("no seam installed for %s", name);
        json_t *r = rsm2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* arg builders: None-valued keys dropped on both sides ("" kept). */
static void rsm2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void rsm2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void rsm2_arg_b(json_t *a, const char *k, bool v)
{
    json_set(a, k, json_bool(v));
}
static void rsm2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* logger seam: {msg, args:[], exc_info}. */
static json_t *rsm2_log(rsm2_io_t *io, const char *level, const char *msg, bool exc_info)
{
    json_t *a = json_object();
    rsm2_arg_s(a, "msg", msg);
    json_set(a, "args", json_array());
    if (exc_info) rsm2_arg_b(a, "exc_info", true);
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    return rsm2_call(io, name, a);
}

/* invoke a callable double {"__callable__": seam} with positional/keyword args.
 * args_arr / kwargs_obj ownership transferred (consumed). */
static json_t *rsm2_invoke_callable(rsm2_io_t *io, const json_t *cb,
                                    json_t *args_arr, json_t *kwargs_obj)
{
    const char *name = rsm2_str(rsm2_get(cb, "__callable__"));
    if (!name) {
        json_free(args_arr);
        json_free(kwargs_obj);
        return rsm2_mkraise("TypeError", "'NoneType' object is not callable");
    }
    json_t *a = json_object();
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
    return rsm2_call(io, name, a);
}

/* ── str() / _text leaf (ASCII scalar domain) ── */
static void rsm2_float_repr(double v, char *out, size_t cap)
{
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}
/* str(v) for a scalar (null -> "None"). Caller frees. */
static char *rsm2_pystr(const json_t *v)
{
    if (rsm2_is_null(v)) return strdup("None");
    if (rsm2_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (rsm2_is_num(v)) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            rsm2_float_repr(d, buf, sizeof buf);
        return strdup(buf);
    }
    if (rsm2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
/* _text(event, key) = str(event.get(key) or ""). malloc'd. */
static char *rsm2_text(const json_t *event, const char *key)
{
    const json_t *v = rsm2_get(event, key);
    if (!rsm2_truth(v)) return strdup("");
    return rsm2_pystr(v);
}

/* ASCII str.strip() whitespace (includes \x1c-\x1f). */
static bool rsm2_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
static char *rsm2_strip_lower(const char *s)
{
    if (!s) return strdup("");
    const char *b = s;
    while (*b && rsm2_is_ws((unsigned char)*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && rsm2_is_ws((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)b[i];
        r[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
    r[n] = '\0';
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 1. _text
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_text_field @ hermes_cli/observability/relay_shared_metrics.py:_text */
static json_t *rsm2_text_field(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    char *s = rsm2_text(rsm2_get(in, "event"), json_get_str(in, "key", ""));
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. _elapsed_ms  — max(0, (monotonic_ns() - started_ns) // 1_000_000)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_elapsed_ms @ hermes_cli/observability/relay_shared_metrics.py:_elapsed_ms */
static json_t *rsm2_elapsed_ms(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *now = rsm2_call(io, "monotonic_ns", json_object());
    if (rsm2_is_raise(now)) return now;
    double now_ns = rsm2_is_num(now) ? now->num_val : 0.0;
    json_free(now);
    double started = rsm2_num(in, "started_ns", 0.0);
    double diff = now_ns - started;
    double q = floor(diff / 1000000.0);   /* Python // floors toward -inf */
    if (q < 0) q = 0;
    return json_number(q);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. _scope_handle — task.handle if task is not None else session.relay_session.handle
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_scope_handle @ hermes_cli/observability/relay_shared_metrics.py:_scope_handle */
static json_t *rsm2_scope_handle(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *task = rsm2_get(in, "task");
    if (!rsm2_is_null(task))
        return rsm2_copy_or_null(rsm2_get(task, "handle"));
    const json_t *session = rsm2_get(in, "session");
    const json_t *relay_session = rsm2_get(session, "relay_session");
    return rsm2_copy_or_null(rsm2_get(relay_session, "handle"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. _sole — the single identity-distinct element, else None
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_sole @ hermes_cli/observability/relay_shared_metrics.py:_sole */
static json_t *rsm2_sole(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *items = rsm2_get(in, "items");
    if (!rsm2_is_arr(items) || items->c.count == 0) return NULL;
    /* unique = {id(item): item}; first occurrence wins; len==1 -> that item. */
    const char *first_ref = rsm2_ref(items->c.items[0]);
    for (size_t i = 1; i < items->c.count; i++) {
        if (!rsm2_str_eq(rsm2_ref(items->c.items[i]), first_ref))
            return NULL;   /* more than one distinct identity */
    }
    return json_copy(items->c.items[0]);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. _forget — drop key from index only while it still points at owner
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_forget @ hermes_cli/observability/relay_shared_metrics.py:_forget */
static json_t *rsm2_forget(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *index = rsm2_get(st, "index");
    const char *key = json_get_str(in, "key", "");
    const json_t *owner = rsm2_get(in, "owner");
    const json_t *cur = rsm2_get(index, key);
    /* index.get(key) is owner — identity by ref; absent (NULL) is not owner. */
    if (cur && !rsm2_is_null(owner) && rsm2_str_eq(rsm2_ref(cur), rsm2_ref(owner)))
        json_obj_del(index, key);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. _task_parent_handle
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_task_parent_handle @ hermes_cli/observability/relay_shared_metrics.py:_task_parent_handle */
static json_t *rsm2_task_parent_handle(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *session = rsm2_get(in, "session");
    char *sid = rsm2_text(session, "session_id");
    const char *task_id = json_get_str(in, "task_id", "");
    json_t *a = json_object();
    rsm2_arg_s(a, "session_id", sid);
    json_t *active = rsm2_call(io, "relay_runtime.active_turn", a);
    free(sid);
    if (rsm2_is_raise(active)) return active;
    json_t *ret = NULL;
    if (!rsm2_is_null(active)) {
        const json_t *lease = rsm2_get(active, "lease");
        const char *lease_sid = rsm2_str(rsm2_get(lease, "session_id"));
        const char *active_tid = rsm2_str(rsm2_get(active, "task_id"));
        const json_t *handle = rsm2_get(active, "handle");
        char *sess_sid = rsm2_text(session, "session_id");
        bool match = rsm2_str_eq(lease_sid, sess_sid)
                  && rsm2_str_eq(active_tid, task_id)
                  && !rsm2_is_null(handle);
        free(sess_sid);
        if (match) ret = json_copy(handle);
    }
    if (!ret) {
        const json_t *relay_session = rsm2_get(session, "relay_session");
        ret = rsm2_copy_or_null(rsm2_get(relay_session, "handle"));
    }
    json_free(active);
    return ret;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 7. _with_launch_entrypoint
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_with_launch_entrypoint @ hermes_cli/observability/relay_shared_metrics.py:_with_launch_entrypoint */
static json_t *rsm2_with_launch_entrypoint(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *event = rsm2_get(in, "event");
    /* entrypoint or parent_task_id or parent_session_id or surface != "cli" */
    bool guard = rsm2_truth(rsm2_get(event, "entrypoint"))
              || rsm2_truth(rsm2_get(event, "parent_task_id"))
              || rsm2_truth(rsm2_get(event, "parent_session_id"));
    if (!guard) {
        json_t *a = json_object();
        rsm2_arg_j(a, "event", event);
        json_t *surface = rsm2_call(io, "contract.execution_surface", a);
        if (rsm2_is_raise(surface)) return surface;
        const char *s = rsm2_str(surface);
        guard = !rsm2_str_eq(s, "cli");
        json_free(surface);
    }
    if (guard) return rsm2_copy_or_null(event);

    json_t *sq = rsm2_call(io, "is_single_query_session", json_object());
    if (rsm2_is_raise(sq)) return sq;
    bool single = rsm2_truth(sq);
    json_free(sq);
    if (!single) return rsm2_copy_or_null(event);

    json_t *ga = json_object();
    rsm2_arg_s(ga, "name", "HERMES_SESSION_SOURCE");
    rsm2_arg_s(ga, "default", "");
    json_t *env = rsm2_call(io, "get_session_env", ga);
    if (rsm2_is_raise(env)) return env;
    /* str(get_session_env(...) or "").strip().lower() */
    char *raw = rsm2_truth(env) ? rsm2_pystr(env) : strdup("");
    json_free(env);
    char *source = rsm2_strip_lower(raw ? raw : "");
    free(raw);
    bool background = rsm2_str_eq(source, "a2a") || rsm2_str_eq(source, "kanban");
    free(source);
    json_t *out = event ? json_copy(event) : json_object();
    json_set(out, "entrypoint", json_string(background ? "background" : "one_shot"));
    return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 8. _cold_key (staticmethod) — get_conversation_context() or session_id
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_cold_key @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._cold_key */
static json_t *rsm2_cold_key(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *ctx = rsm2_call(io, "get_conversation_context", json_object());
    if (rsm2_is_raise(ctx)) return ctx;
    if (rsm2_truth(ctx)) return ctx;   /* truthy -> return it */
    json_free(ctx);
    return json_string(json_get_str(in, "session_id", ""));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 9. _process_marks_admitted — turn is None or turn.relay_enabled
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_process_marks_admitted @ hermes_cli/observability/relay_shared_metrics.py:_process_marks_admitted */
static json_t *rsm2_process_marks_admitted(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *turn = rsm2_call(io, "relay_runtime.current_turn", json_object());
    if (rsm2_is_raise(turn)) return turn;
    if (rsm2_is_null(turn)) { json_free(turn); return json_bool(true); }
    const json_t *re = rsm2_get(turn, "relay_enabled");
    if (!re) {
        json_free(turn);
        return rsm2_mkraise("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'relay_enabled'");
    }
    bool enabled = rsm2_truth(re);
    json_free(turn);
    return json_bool(enabled);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 10. _raw_config — read_raw_config_readonly() or {}
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_raw_config @ hermes_cli/observability/relay_shared_metrics.py:_raw_config */
static json_t *rsm2_raw_config(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *cfg = rsm2_call(io, "read_raw_config_readonly", json_object());
    if (rsm2_is_raise(cfg)) return cfg;
    if (!rsm2_truth(cfg)) { json_free(cfg); return json_object(); }
    return cfg;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 11. _resolved_send_config — resolve_send_config(_raw_config())
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_resolved_send_config @ hermes_cli/observability/relay_shared_metrics.py:_resolved_send_config */
static json_t *rsm2_resolved_send_config(rsm2_io_t *io, const json_t *in, json_t *st)
{
    json_t *cfg = rsm2_raw_config(io, in, st);   /* REAL composition */
    if (rsm2_is_raise(cfg)) return cfg;
    json_t *a = json_object();
    rsm2_arg_j(a, "config", cfg);
    json_free(cfg);
    return rsm2_call(io, "resolve_send_config", a);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 12. _guarded (staticmethod) — run callback; log-and-swallow Exception
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rsm2_guarded_core(rsm2_io_t *io, const char *message,
                                 const json_t *callback, json_t *args_arr, json_t *kwargs_obj)
{
    json_t *r = rsm2_invoke_callable(io, callback, args_arr, kwargs_obj);
    if (rsm2_is_raise(r)) {
        if (rsm2_is_base(rsm2_kind(r))) return r;   /* BaseException passthrough */
        json_free(r);
        json_t *lr = rsm2_log(io, "warning", message, true);
        if (rsm2_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;   /* swallowed -> None */
    }
    return r;
}
/* PoP: rsm2_guarded @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._guarded */
static json_t *rsm2_guarded(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *message = json_get_str(in, "message", "");
    const json_t *callback = rsm2_get(in, "callback");
    json_t *args_arr = json_array();
    const json_t *ia = rsm2_get(in, "args");
    if (rsm2_is_arr(ia))
        for (size_t i = 0; i < ia->c.count; i++)
            json_append(args_arr, json_copy(ia->c.items[i]));
    json_t *kwargs_obj = json_object();
    const json_t *ik = rsm2_get(in, "kwargs");
    if (rsm2_is_obj(ik))
        for (size_t i = 0; i < ik->c.count; i++)
            json_set(kwargs_obj, ik->c.keys[i], json_copy(ik->c.items[i]));
    return rsm2_guarded_core(io, message, callback, args_arr, kwargs_obj);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 13. _with_scope_stack — relay.get_scope_stack(); return callback(*a, **k)
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rsm2_with_scope_stack_core(rsm2_io_t *io, const json_t *callback,
                                          json_t *args_arr, json_t *kwargs_obj)
{
    json_t *gs = rsm2_call(io, "relay.get_scope_stack", json_object());
    if (rsm2_is_raise(gs)) {
        json_free(args_arr);
        json_free(kwargs_obj);
        return gs;
    }
    json_free(gs);
    return rsm2_invoke_callable(io, callback, args_arr, kwargs_obj);
}
/* PoP: rsm2_with_scope_stack @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._with_scope_stack */
static json_t *rsm2_with_scope_stack(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *callback = rsm2_get(in, "callback");
    json_t *args_arr = json_array();
    const json_t *ia = rsm2_get(in, "args");
    if (rsm2_is_arr(ia))
        for (size_t i = 0; i < ia->c.count; i++)
            json_append(args_arr, json_copy(ia->c.items[i]));
    json_t *kwargs_obj = json_object();
    const json_t *ik = rsm2_get(in, "kwargs");
    if (rsm2_is_obj(ik))
        for (size_t i = 0; i < ik->c.count; i++)
            json_set(kwargs_obj, ik->c.keys[i], json_copy(ik->c.items[i]));
    return rsm2_with_scope_stack_core(io, callback, args_arr, kwargs_obj);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 14. _emit_rows — guarded mark per row (process mark when session is None)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_emit_rows @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._emit_rows */
static json_t *rsm2_emit_rows(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *session = rsm2_get(in, "session");
    const json_t *rows = rsm2_get(in, "rows");
    bool has_session = !rsm2_is_null(session);
    const char *msg = "Hermes shared-metrics efficiency mark failed";
    if (!rsm2_is_arr(rows)) return NULL;
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        const json_t *mark = rsm2_is_arr(row) ? json_get(row, 0) : NULL;
        const json_t *data = rsm2_is_arr(row) ? json_get(row, 1) : NULL;
        json_t *cb = json_object();
        json_t *args_arr = json_array();
        if (!has_session) {
            json_set(cb, "__callable__", json_string("rt.record_process_mark"));
            json_append(args_arr, rsm2_copy_or_null(mark));
            json_append(args_arr, rsm2_copy_or_null(data));
        } else {
            json_set(cb, "__callable__", json_string("rt._mark"));
            json_append(args_arr, json_copy(session));
            json_append(args_arr, json_null());   /* task = None */
            json_append(args_arr, rsm2_copy_or_null(mark));
            json_append(args_arr, rsm2_copy_or_null(data));
        }
        json_t *r = rsm2_guarded_core(io, msg, cb, args_arr, json_object());
        json_free(cb);
        if (rsm2_is_raise(r)) return r;   /* BaseException passthrough */
        json_free(r);
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 15. _any_session — _task_session(event, allow_task_id_fallback=True) or _session(event)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_any_session @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._any_session */
static json_t *rsm2_any_session(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *event = rsm2_get(in, "event");
    json_t *a = json_object();
    rsm2_arg_j(a, "event", event);
    rsm2_arg_b(a, "allow_task_id_fallback", true);
    json_t *owner = rsm2_call(io, "rt._task_session", a);
    if (rsm2_is_raise(owner)) return owner;
    if (rsm2_truth(owner)) return owner;
    json_free(owner);
    json_t *b = json_object();
    rsm2_arg_j(b, "event", event);
    return rsm2_call(io, "rt._session", b);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 16. _task_pair — resolve (session, task); start opens a missing task
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_task_pair @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._task_pair */
static json_t *rsm2_task_pair(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *event = rsm2_get(in, "event");
    bool start = rsm2_bool(in, "start", false);
    const json_t *lookup = rsm2_get(in, "lookup");

    json_t *a = json_object();
    rsm2_arg_j(a, "event", event);
    if (rsm2_is_obj(lookup))
        for (size_t i = 0; i < lookup->c.count; i++)
            json_set(a, lookup->c.keys[i], json_copy(lookup->c.items[i]));
    json_t *session = rsm2_call(io, "rt._task_session", a);
    if (rsm2_is_raise(session)) return session;

    char *tid = rsm2_text(event, "task_id");
    json_t *task = NULL;
    if (!rsm2_is_null(session)) {
        const json_t *tasks = rsm2_get(session, "tasks");
        task = rsm2_copy_or_null(rsm2_get(tasks, tid));
    }
    free(tid);

    if (rsm2_is_null(task) && start) {
        json_t *sa = json_object();
        rsm2_arg_j(sa, "event", event);
        json_t *started = rsm2_call(io, "rt.start_task", sa);
        if (rsm2_is_raise(started)) { json_free(session); return started; }
        task = started;   /* owned */
        json_free(session);
        session = NULL;
        if (!rsm2_is_null(task)) {
            json_t *b = json_object();
            rsm2_arg_j(b, "event", event);
            session = rsm2_call(io, "rt._task_session", b);
            if (rsm2_is_raise(session)) { json_free(task); return session; }
        }
    }
    json_t *out = json_array();
    json_append(out, session ? session : json_null());
    json_append(out, task ? task : json_null());
    return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 17. _abort_session — mark closing; system-abort open tasks
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_abort_session @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._abort_session */
static json_t *rsm2_abort_session(rsm2_io_t *io, const json_t *in, json_t *st)
{
    json_t *session = rsm2_get(st, "session");
    if (rsm2_truth(rsm2_get(session, "closing"))) return json_bool(false);
    json_set(session, "closing", json_bool(true));
    const json_t *base_event = rsm2_get(in, "base_event");
    const json_t *tasks = rsm2_get(session, "tasks");
    /* snapshot list(session.tasks) */
    size_t n = rsm2_is_obj(tasks) ? tasks->c.count : 0;
    char **keys = NULL;
    if (n) {
        keys = (char **)malloc(n * sizeof(char *));
        if (keys) for (size_t i = 0; i < n; i++) keys[i] = strdup(tasks->c.keys[i]);
    }
    for (size_t i = 0; i < n; i++) {
        json_t *event = base_event ? json_copy(base_event) : json_object();
        json_set(event, "task_id", json_string(keys[i]));
        json_t *a = json_object();
        rsm2_arg_j(a, "session", session);
        rsm2_arg_s(a, "task_id", keys[i]);
        rsm2_arg_j(a, "event", event);
        json_free(event);
        json_t *r = rsm2_call(io, "rt._finish_task", a);
        if (rsm2_is_raise(r)) {
            for (size_t j = i; j < n; j++) free(keys[j]);
            free(keys);
            return r;
        }
        json_free(r);
    }
    for (size_t j = 0; j < n; j++) free(keys[j]);
    free(keys);
    json_t *b = json_object();
    rsm2_arg_j(b, "session", session);
    rsm2_arg_j(b, "event", base_event);
    json_t *r = rsm2_call(io, "rt._end_pending_model_calls", b);
    if (rsm2_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 18. start_user_turn — task = start_task(event); task.cost.user_turn = True
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_start_user_turn @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.start_user_turn */
static json_t *rsm2_start_user_turn(rsm2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *event = rsm2_get(in, "event");
    json_t *a = json_object();
    rsm2_arg_j(a, "event", event);
    json_t *task = rsm2_call(io, "rt.start_task", a);
    if (rsm2_is_raise(task)) return task;
    if (rsm2_is_null(task)) { json_free(task); return NULL; }
    json_t *cost = rsm2_get(task, "cost");
    if (rsm2_is_obj(cost))
        json_set(cost, "user_turn", json_bool(true));
    json_set(st, "task", task);   /* transfer ownership; echo mutated task */
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 19. _deregister — _safe(deregister, name); release_managed_execution; flag
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_deregister @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._deregister */
static json_t *rsm2_deregister(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    const char *name = json_get_str(st, "_subscriber_name", "");
    /* self._safe(self.relay.subscribers.deregister, name) — _safe is a seam. */
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("relay.subscribers.deregister"));
    json_t *sa = json_object();
    rsm2_arg_j(sa, "callback", cb);
    json_free(cb);
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(name));
    json_set(sa, "args", args_arr);
    json_set(sa, "kwargs", json_object());
    json_t *r = rsm2_call(io, "rt._safe", sa);
    if (rsm2_is_raise(r)) return r;
    json_free(r);
    json_t *ha = json_object();
    rsm2_arg_s(ha, "name", name);
    json_t *hr = rsm2_call(io, "host.release_managed_execution", ha);
    if (rsm2_is_raise(hr)) return hr;
    json_free(hr);
    json_set(st, "_registered", json_bool(false));
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 20. _join_send_thread — bounded join; log-and-swallow Exception
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *rsm2_join_send_thread_core(rsm2_io_t *io, json_t *st, double timeout)
{
    const json_t *thread = rsm2_get(st, "_send_thread");
    if (rsm2_is_null(thread)) return NULL;
    json_t *alive = rsm2_call(io, "thread.is_alive", json_object());
    if (rsm2_is_raise(alive)) return alive;
    bool is_alive = rsm2_truth(alive);
    json_free(alive);
    if (!is_alive) return NULL;
    json_t *ja = json_object();
    rsm2_arg_n(ja, "timeout", timeout);
    json_t *jr = rsm2_call(io, "thread.join", ja);
    if (rsm2_is_raise(jr)) {
        if (rsm2_is_base(rsm2_kind(jr))) return jr;   /* passthrough */
        json_free(jr);
        json_t *lr = rsm2_log(io, "debug", "Shared-metrics send thread join failed", true);
        if (rsm2_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    json_free(jr);
    return NULL;
}
/* PoP: rsm2_join_send_thread @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._join_send_thread */
static json_t *rsm2_join_send_thread(rsm2_io_t *io, const json_t *in, json_t *st)
{
    double timeout = rsm2_num(in, "timeout", 2.0);
    return rsm2_join_send_thread_core(io, st, timeout);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 21. _release — _join_send_thread(); suppress(Exception) atexit.unregister
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_release @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._release */
static json_t *rsm2_release(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *r = rsm2_join_send_thread_core(io, st, 2.0);   /* REAL composition */
    if (rsm2_is_raise(r)) return r;
    json_free(r);
    /* with contextlib.suppress(Exception): atexit.unregister(self.shutdown) */
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("rt.shutdown"));
    json_t *a = json_object();
    rsm2_arg_j(a, "callback", cb);
    json_free(cb);
    json_t *ur = rsm2_call(io, "atexit.unregister", a);
    if (rsm2_is_raise(ur)) {
        if (rsm2_is_base(rsm2_kind(ur))) return ur;   /* passthrough */
        json_free(ur);   /* Exception suppressed */
    } else {
        json_free(ur);
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 22. _lineage_spent — lineages[lineage_of.get(session_id, "")].spent or None
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_lineage_spent @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._lineage_spent */
static json_t *rsm2_lineage_spent(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *session_id = json_get_str(in, "session_id", "");
    const json_t *lineage_of = rsm2_get(st, "_lineage_of");
    const char *key = rsm2_str(rsm2_get(lineage_of, session_id));
    if (!key) key = "";   /* .get(session_id, "") */
    const json_t *lineages = rsm2_get(st, "_lineages");
    const json_t *lineage = rsm2_get(lineages, key);
    if (rsm2_is_null(lineage)) return NULL;
    return rsm2_copy_or_null(rsm2_get(lineage, "spent"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 23. _close_child_session — close_session({session_id}) when child id present
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_close_child_session @ hermes_cli/observability/relay_shared_metrics.py:_close_child_session */
static json_t *rsm2_close_child_session(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    char *child = rsm2_text(rsm2_get(in, "kwargs"), "child_session_id");
    if (!child[0]) { free(child); return NULL; }
    json_t *event = json_object();
    json_set(event, "session_id", json_string(child));
    free(child);
    json_t *a = json_object();
    rsm2_arg_j(a, "event", event);
    json_free(event);
    json_t *r = rsm2_call(io, "rt.close_session", a);
    if (rsm2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── module-level runtime-gate helpers ── */
/* isinstance(runtime, _Runtime): a registry value marked {"__runtime__": true}. */
static bool rsm2_is_runtime(const json_t *v)
{
    return rsm2_is_obj(v) && rsm2_bool(v, "__runtime__", false);
}
/* runtime._safe(callback, *args, **kwargs) — seam; callback projected as a
 * callable double, runtime_id carried for trace disambiguation. */
static json_t *rsm2_rt_safe(rsm2_io_t *io, const json_t *runtime, const char *cb_name,
                            json_t *args_arr, json_t *kwargs_obj)
{
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string(cb_name));
    json_t *a = json_object();
    rsm2_arg_j(a, "callback", cb);
    json_free(cb);
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
    const char *rid = rsm2_str(rsm2_get(runtime, "id"));
    rsm2_arg_s(a, "runtime_id", rid);
    return rsm2_call(io, "rt._safe", a);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 24. _run_task_hook — enabled gate; _get_runtime; _safe(getattr(rt, method))
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_run_task_hook @ hermes_cli/observability/relay_shared_metrics.py:_run_task_hook */
static json_t *rsm2_run_task_hook(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *en = rsm2_call(io, "enabled", json_object());
    if (rsm2_is_raise(en)) return en;
    bool enabled = rsm2_truth(en);
    json_free(en);
    if (!enabled) return NULL;
    bool retry_failed = rsm2_bool(in, "retry_failed", false);
    json_t *ga = json_object();
    rsm2_arg_b(ga, "retry_failed", retry_failed);
    json_t *runtime = rsm2_call(io, "_get_runtime", ga);
    if (rsm2_is_raise(runtime)) return runtime;
    if (rsm2_is_null(runtime)) { json_free(runtime); return NULL; }
    const char *method = json_get_str(in, "method", "");
    char *cb_name = rsm2_xfmt("rt.%s", method);
    json_t *args_arr = json_array();
    json_append(args_arr, rsm2_copy_or_null(rsm2_get(in, "event")));
    json_t *r = rsm2_rt_safe(io, runtime, cb_name ? cb_name : "rt.", args_arr, json_object());
    free(cb_name);
    json_free(runtime);
    if (rsm2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 25. record_process_mark (module) — enabled + admitted gates; _get_runtime; _safe
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_record_process_mark @ hermes_cli/observability/relay_shared_metrics.py:record_process_mark */
static json_t *rsm2_record_process_mark(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *en = rsm2_call(io, "enabled", json_object());
    if (rsm2_is_raise(en)) return en;
    bool gate = rsm2_truth(en);
    json_free(en);
    if (gate) {
        json_t *ad = rsm2_process_marks_admitted(io, in, st);   /* REAL composition */
        if (rsm2_is_raise(ad)) return ad;
        gate = rsm2_truth(ad);
        json_free(ad);
    }
    if (!gate) return NULL;
    json_t *ga = json_object();
    rsm2_arg_b(ga, "retry_failed", true);
    json_t *runtime = rsm2_call(io, "_get_runtime", ga);
    if (rsm2_is_raise(runtime)) return runtime;
    if (rsm2_is_null(runtime)) { json_free(runtime); return NULL; }
    json_t *args_arr = json_array();
    json_append(args_arr, rsm2_copy_or_null(rsm2_get(in, "mark")));
    json_append(args_arr, rsm2_copy_or_null(rsm2_get(in, "data")));
    json_t *r = rsm2_rt_safe(io, runtime, "rt.record_process_mark", args_arr, json_object());
    json_free(runtime);
    if (rsm2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 26. record_process_marks_saved (module) — gates; len(marks) fallback; _safe or 0
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_record_process_marks_saved @ hermes_cli/observability/relay_shared_metrics.py:record_process_marks_saved */
static json_t *rsm2_record_process_marks_saved(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *marks = rsm2_get(in, "marks");
    double nmarks = rsm2_is_arr(marks) ? (double)marks->c.count : 0.0;
    json_t *en = rsm2_call(io, "enabled", json_object());
    if (rsm2_is_raise(en)) return en;
    bool gate = rsm2_truth(en);
    json_free(en);
    if (gate) {
        json_t *ad = rsm2_process_marks_admitted(io, in, st);   /* REAL composition */
        if (rsm2_is_raise(ad)) return ad;
        gate = rsm2_truth(ad);
        json_free(ad);
    }
    if (!gate) return json_number(nmarks);
    json_t *ga = json_object();
    rsm2_arg_b(ga, "retry_failed", true);
    json_t *runtime = rsm2_call(io, "_get_runtime", ga);
    if (rsm2_is_raise(runtime)) return runtime;
    if (rsm2_is_null(runtime)) { json_free(runtime); return json_number(0); }
    json_t *args_arr = json_array();
    json_append(args_arr, rsm2_copy_or_null(marks));
    json_t *r = rsm2_rt_safe(io, runtime, "rt.record_process_marks_saved", args_arr, json_object());
    json_free(runtime);
    if (rsm2_is_raise(r)) return r;
    /* (runtime._safe(...) or 0) */
    double saved = rsm2_truth(r) ? (rsm2_is_num(r) ? r->num_val : 0.0) : 0.0;
    json_free(r);
    return json_number(saved);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 27. close_session_run — session_id + enabled gates; registry lookup; _safe
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_close_session_run @ hermes_cli/observability/relay_shared_metrics.py:close_session_run */
static json_t *rsm2_close_session_run(rsm2_io_t *io, const json_t *in, json_t *st)
{
    const char *session_id = json_get_str(in, "session_id", "");
    if (!session_id[0]) return NULL;
    json_t *en = rsm2_call(io, "enabled", json_object());
    if (rsm2_is_raise(en)) return en;
    bool enabled = rsm2_truth(en);
    json_free(en);
    if (!enabled) return NULL;
    json_t *pk = rsm2_call(io, "relay_runtime.current_profile_key", json_object());
    if (rsm2_is_raise(pk)) return pk;
    const char *key = rsm2_str(pk);
    const json_t *registry = rsm2_get(st, "_RUNTIMES");
    const json_t *runtime = rsm2_get(registry, key ? key : "");
    json_free(pk);
    if (!rsm2_is_runtime(runtime)) return NULL;
    json_t *event = json_object();
    json_set(event, "session_id", json_string(session_id));
    json_t *args_arr = json_array();
    json_append(args_arr, event);
    json_t *r = rsm2_rt_safe(io, runtime, "rt.close_session", args_arr, json_object());
    if (rsm2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 28. rotate_segment (module) — id guards + enabled; registry lookup; _safe
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_rotate_segment @ hermes_cli/observability/relay_shared_metrics.py:rotate_segment */
static json_t *rsm2_rotate_segment(rsm2_io_t *io, const json_t *in, json_t *st)
{
    const char *old_id = json_get_str(in, "old_session_id", "");
    const char *new_id = json_get_str(in, "new_session_id", "");
    if (!old_id[0] || !new_id[0] || rsm2_str_eq(old_id, new_id)) return NULL;
    json_t *en = rsm2_call(io, "enabled", json_object());
    if (rsm2_is_raise(en)) return en;
    bool enabled = rsm2_truth(en);
    json_free(en);
    if (!enabled) return NULL;
    json_t *pk = rsm2_call(io, "relay_runtime.current_profile_key", json_object());
    if (rsm2_is_raise(pk)) return pk;
    const char *key = rsm2_str(pk);
    const json_t *registry = rsm2_get(st, "_RUNTIMES");
    const json_t *runtime = rsm2_get(registry, key ? key : "");
    json_free(pk);
    if (!rsm2_is_runtime(runtime)) return NULL;
    json_t *args_arr = json_array();
    json_append(args_arr, json_string(old_id));
    json_append(args_arr, json_string(new_id));
    json_t *r = rsm2_rt_safe(io, runtime, "rt.rotate_segment", args_arr, json_object());
    if (rsm2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 29. shutdown_runtimes — filter live runtimes; _safe(shutdown) each
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: rsm2_shutdown_runtimes @ hermes_cli/observability/relay_shared_metrics.py:shutdown_runtimes */
static json_t *rsm2_shutdown_runtimes(rsm2_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    const json_t *registry = rsm2_get(st, "_RUNTIMES");
    if (!rsm2_is_obj(registry)) return NULL;
    /* snapshot the live runtimes in registry insertion order */
    size_t n = registry->c.count;
    for (size_t i = 0; i < n; i++) {
        const json_t *runtime = registry->c.items[i];
        if (!rsm2_is_runtime(runtime)) continue;
        json_t *r = rsm2_rt_safe(io, runtime, "rt.shutdown", json_array(), json_object());
        if (rsm2_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}