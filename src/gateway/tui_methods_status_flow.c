/*
 * tui_methods_status_flow.c — FLOW + leaf port of the TUI session live-status
 * and registry surface (hermes-agent).
 *
 * MODULE RESOLUTION: the task named tui_gateway/methods_status.py, which does
 * NOT exist in the quarry. The TUI's live session STATUS methods
 * (_session_live_status / _session_pending_kind / _session_live_title) and the
 * registry claim/reuse + build-wait + RPC session-resolver flows that feed them
 * are defined in tui_gateway/session_registry.py (fully UNCLAIMED). That is the
 * REAL defining module; every PoP below credits it (the scanner only credits
 * the defining module). The session.status RPC handler itself lives in
 * methods_session.py and its _status_row/_try_get_session helpers are already
 * claimed; this port targets the unclaimed registry/status cluster instead.
 *
 * Ports the DECISION SEQUENCES — branch order, seam call order, module-global
 * (_sessions) state transitions, identity checks and exception paths — of:
 *   _wait_agent, _agent_build_wait_cap, _bind_build_profile_scopes,
 *   _release_build_profile_scopes, _deferred_build_agent_kwargs,
 *   _await_resume_history, _attach_built_agent, _start_session_services,
 *   _finish_agent_build, _sess_nowait, _sess_building, _sess, _new_session_key,
 *   _is_server_minted_key, _any_live_session_claims_key, _live_profile_matches,
 *   _session_lookup_key, _find_live_session_by_key, _claim_parked_runtimes,
 *   _finalize_superseded_runtimes, _claim_or_reuse_live, _session_pending_kind,
 *   _session_live_status, _session_live_title, _durable_ids,
 *   _legacy_display_anchor, _reconcile_display_with_live,
 *   _main_runtime_from_agent.
 *
 * Every live EXTERNAL call enters through ONE universal seam:
 * tmst_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * SIBLING MODELING: ported FLOW siblings (_sess_nowait, _sess_building,
 * _wait_agent, _session_pending_kind, _find_live_session_by_key,
 * _claim_parked_runtimes, _finalize_superseded_runtimes,
 * _release_build_profile_scopes) are SEAMED inside other flows (scripted) and
 * run REAL in their own direct cases. Ported PURE-LEAF siblings
 * (_session_lookup_key, _live_profile_matches, _durable_ids,
 * _legacy_display_anchor) run REAL inside flows (they are deterministic given
 * the same inputs; _legacy_display_anchor's _coerce_message_text external seam
 * fires in the parent trace exactly as live).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmst_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   _err                       server._err(rid, code, msg) -> error envelope
 *   _load_cfg                  server._load_cfg() -> cfg dict
 *   agent_ready.wait           session["agent_ready"].wait(timeout=) -> bool
 *   agent_ready.is_set         session["agent_ready"].is_set() -> bool
 *   resume_history.wait        current["resume_history_ready"].wait(timeout=300.0)
 *   old_stop.set               rec["_notif_stop"].set()
 *   lease.release              lease.release()
 *   db.close                   session_db.close()
 *   _profile_runtime_scope_tokens  model_switch._profile_runtime_scope_tokens(home)
 *   scope_for_profile          agent.runtime_scope.scope_for_profile(home)
 *   bind_state                 agent.runtime_scope.bind_state(scope, secrets={})
 *   _release_profile_runtime_scope_tokens  model_switch._release_...(scopes)
 *   _context_cwd_is_launch_artifact  session_workdir._context_cwd_is_launch_artifact(current)
 *   _session_source            session_workdir._session_source(current)
 *   _session_cwd               session_workdir._session_cwd(current)
 *   _overrides_have_routable_provider  agent_factory._overrides_have_routable_provider(ro)
 *   _register_session_cwd      server._register_session_cwd(record)
 *   _session_todo_state        server._session_todo_state(record)
 *   _config_model_target       server._config_model_target()
 *   _start_notification_poller server._start_notification_poller(sid, rec) -> stop
 *   _notify_session_boundary   server._notify_session_boundary(hook, key, source)
 *   _start_agent_build         session_registry._start_agent_build(sid, session)
 *   _transfer_db_to_agent      server._transfer_db_to_agent(agent, db) -> bool
 *   _release_persistence       server._release_persistence(persistence)
 *   unregister_gateway_notify  tools.approval.unregister_gateway_notify(key)
 *   new_session_id             server.new_session_id() -> str
 *   pending_kind               tui_gateway.server_requests.pending_kind(sid) -> str
 *   _coerce_message_text       session_history._coerce_message_text(content) -> str
 *   session_db                 server._session_db(session) CM -> db (enter/exit silent)
 *   persistence_for            server._persistence_for(db) CM -> persistence (silent)
 *   get_title                  persistence.sessions.get_title(key) -> str|null
 *   _cancel_ws_orphan_reap     server._cancel_ws_orphan_reap(sid)
 *   _pop_session_by_id         server._pop_session_by_id(sid) -> record|null
 *                              [side effect: removes sid from _sessions when non-null]
 *   _teardown_popped_session   server._teardown_popped_session(popped, end_reason=)
 *   _current_rpc_method.get    server._current_rpc_method.get() -> str|null
 *   log.warning                logger.warning(fmt, *args) [fmt+args verbatim]
 *   log.exception              logger.exception(fmt, *args) [fmt+args verbatim]
 *   <ported flow sibling>      the sibling live call (scripted inside flows)
 *
 * Representation contract (shared with tests/oracle_tui_methods_status_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception / contextlib.suppress(Exception) site. The
 *   FULL builtin hierarchy rides the tmst_exc_parents table; an unknown scripted
 *   kind defaults to Exception (the oracle make_exc fallback).
 * - Doubles: session/current/record are fixture DICTS (live .get); agent is a
 *   fixture dict materialized to a SimpleNamespace live (getattr/setattr);
 *   lease/db/persistence/scope/stop are opaque markers. A callable api_key is
 *   the marker {"__callable__": true} (canon projects callables back to it).
 * - _ANY_PROFILE sentinel is the args marker "__ANY_PROFILE__".
 * - Identity (`_sessions.get(sid) is current`): args.current_sid names the
 *   registry record that IS current (identity match iff sid == current_sid and
 *   present); absent/null current_sid => current is args.current (standalone,
 *   never identity-matches). Mutations land on the registry only on a match.
 * - State: ctx.state.sessions mirrors the module-global _sessions dict
 *   (sid -> record). Functions that touch _sessions materialize the key; others
 *   leave state alone. _pop_session_by_id removes the sid on a non-null result.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints;
 *   Paths/str stay POSIX (ASCII fixture corpus). str()/strip() fold ASCII only.
 * - Locks (_sessions_lock / _session_resume_lock / history_lock) and CM
 *   enter/exit are SILENT (no trace events); only the yielded value / wrapped
 *   call is scripted.
 *
 * Ownership: tmst_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node — borrowed
 * pointers to a replaced key are copied first. Seam results are caller-freed.
 * Input args are BORROWED (never freed/mutated by the port); agent doubles are
 * copied before mutation so the echoed input stays pristine.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct tmst_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmst_ctx_t;

/* ── basic helpers ── */
static bool tmst_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmst_raise_kind(const json_t *v) {
    return tmst_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmst_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmst_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmst_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static char *tmst_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers serialize as JSON; outside the
 * str()-projection fixture domain). */
static char *tmst_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tmst_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tmst_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tmst_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tmst_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tmst_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return false;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type != b->type) return false;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!tmst_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tmst_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* Python str.strip() over the ASCII whitespace set. Caller frees. */
static char *tmst_strip(const char *s) {
    if (!s) s = "";
    const char *start = s;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' ||
                      *start == '\r' || *start == '\v' || *start == '\f'))
        start++;
    const char *end = s + strlen(s);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                           end[-1] == '\r' || end[-1] == '\v' || end[-1] == '\f'))
        end--;
    size_t n = (size_t)(end - start);
    char *o = malloc(n + 1);
    if (o) { memcpy(o, start, n); o[n] = '\0'; }
    return o;
}

/* Python float(v) for the fixture domain (number / numeric string). ok=false
 * models ValueError/TypeError (caught by the surrounding suppress). */
static double tmst_py_float(const json_t *v, bool *ok) {
    *ok = false;
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) { *ok = true; return v->num_val; }
    if (v->type == JSON_BOOL) { *ok = true; return v->bool_val ? 1.0 : 0.0; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *endp = NULL;
        double d = strtod(s, &endp);
        if (endp == s) return 0.0;
        while (*endp == ' ' || *endp == '\t' || *endp == '\n' ||
               *endp == '\r' || *endp == '\v' || *endp == '\f')
            endp++;
        if (*endp != '\0') return 0.0;
        *ok = true;
        return d;
    }
    return 0.0;
}

/* ── exception hierarchy (FULL builtin tree) ──
 * An unknown scripted kind defaults to Exception — exactly the oracle
 * make_exc fallback type(kind, (Exception,)). BaseException-only kinds never
 * reach Exception. */
typedef struct tmst_exc_parent {
    const char *kind;
    const char *parents[4];   /* NULL-terminated */
} tmst_exc_parent_t;

static const tmst_exc_parent_t TMST_EXC_PARENTS[] = {
    {"BaseException", {NULL}},
    {"Exception", {"BaseException", NULL}},
    {"SystemExit", {"BaseException", NULL}},
    {"KeyboardInterrupt", {"BaseException", NULL}},
    {"GeneratorExit", {"BaseException", NULL}},
    {"CancelledError", {"BaseException", NULL}},
    {"StopIteration", {"Exception", NULL}},
    {"StopAsyncIteration", {"Exception", NULL}},
    {"ArithmeticError", {"Exception", NULL}},
    {"FloatingPointError", {"ArithmeticError", NULL}},
    {"OverflowError", {"ArithmeticError", NULL}},
    {"ZeroDivisionError", {"ArithmeticError", NULL}},
    {"AssertionError", {"Exception", NULL}},
    {"AttributeError", {"Exception", NULL}},
    {"BufferError", {"Exception", NULL}},
    {"EOFError", {"Exception", NULL}},
    {"ImportError", {"Exception", NULL}},
    {"ModuleNotFoundError", {"ImportError", NULL}},
    {"LookupError", {"Exception", NULL}},
    {"IndexError", {"LookupError", NULL}},
    {"KeyError", {"LookupError", NULL}},
    {"MemoryError", {"Exception", NULL}},
    {"NameError", {"Exception", NULL}},
    {"UnboundLocalError", {"NameError", NULL}},
    {"OSError", {"Exception", NULL}},
    {"BlockingIOError", {"OSError", NULL}},
    {"ChildProcessError", {"OSError", NULL}},
    {"ConnectionError", {"OSError", NULL}},
    {"BrokenPipeError", {"ConnectionError", NULL}},
    {"ConnectionAbortedError", {"ConnectionError", NULL}},
    {"ConnectionRefusedError", {"ConnectionError", NULL}},
    {"ConnectionResetError", {"ConnectionError", NULL}},
    {"FileExistsError", {"OSError", NULL}},
    {"FileNotFoundError", {"OSError", NULL}},
    {"InterruptedError", {"OSError", NULL}},
    {"IsADirectoryError", {"OSError", NULL}},
    {"NotADirectoryError", {"OSError", NULL}},
    {"PermissionError", {"OSError", NULL}},
    {"ProcessLookupError", {"OSError", NULL}},
    {"TimeoutError", {"OSError", NULL}},
    {"ReferenceError", {"Exception", NULL}},
    {"RuntimeError", {"Exception", NULL}},
    {"NotImplementedError", {"RuntimeError", NULL}},
    {"RecursionError", {"RuntimeError", NULL}},
    {"SyntaxError", {"Exception", NULL}},
    {"IndentationError", {"SyntaxError", NULL}},
    {"TabError", {"IndentationError", NULL}},
    {"SystemError", {"Exception", NULL}},
    {"TypeError", {"Exception", NULL}},
    {"ValueError", {"Exception", NULL}},
    {"UnicodeError", {"ValueError", NULL}},
    {"UnicodeDecodeError", {"UnicodeError", NULL}},
    {"UnicodeEncodeError", {"UnicodeError", NULL}},
    {"UnicodeTranslateError", {"UnicodeError", NULL}},
    {"Warning", {"Exception", NULL}},
    {"DeprecationWarning", {"Warning", NULL}},
    {"PendingDeprecationWarning", {"Warning", NULL}},
    {"RuntimeWarning", {"Warning", NULL}},
    {"SyntaxWarning", {"Warning", NULL}},
    {"UserWarning", {"Warning", NULL}},
    {"FutureWarning", {"Warning", NULL}},
    {"ImportWarning", {"Warning", NULL}},
    {"UnicodeWarning", {"Warning", NULL}},
    {"BytesWarning", {"Warning", NULL}},
    {"ResourceWarning", {"Warning", NULL}},
    {"EncodingWarning", {"Warning", NULL}},
};
#define TMST_EXC_N (sizeof(TMST_EXC_PARENTS) / sizeof(TMST_EXC_PARENTS[0]))

static const tmst_exc_parent_t *tmst_exc_entry(const char *kind) {
    for (size_t i = 0; i < TMST_EXC_N; i++)
        if (!strcmp(TMST_EXC_PARENTS[i].kind, kind)) return &TMST_EXC_PARENTS[i];
    return NULL;
}

/* isinstance(kind, ancestor) over the parent tree; unknown kinds are
 * Exception subclasses (the make_exc fallback). */
static bool tmst_exc_isa(const char *kind, const char *ancestor) {
    if (!kind) return false;
    if (!strcmp(kind, ancestor)) return true;
    const tmst_exc_parent_t *e = tmst_exc_entry(kind);
    if (!e) return !strcmp(ancestor, "Exception") || !strcmp(ancestor, "BaseException");
    for (size_t i = 0; i < 4 && e->parents[i]; i++)
        if (tmst_exc_isa(e->parents[i], ancestor)) return true;
    return false;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tmst_is_base_exc(const char *kind) {
    return kind && !tmst_exc_isa(kind, "Exception");
}

/* ── the one universal seam (fail-closed) ── */
static json_t *tmst_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmst_err_spec(type, msg);
}

static json_t *tmst_call(tmst_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    /* drop null-valued arg keys on both sides (absent-vs-null parity) */
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = tmst_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmst_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tmst_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* args builders (values borrowed; copied into the args object) */
static json_t *tmst_args0(void) { return json_object(); }
static void tmst_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmst_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmst_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmst_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── state helpers ── */
/* Materialize and return the _sessions mirror (owned by ctx.state). */
static json_t *tmst_sessions(tmst_ctx_t *ctx) {
    json_t *s = json_obj_get(ctx->state, "sessions");
    if (!s || s->type != JSON_OBJECT) {
        s = json_object();
        json_set(ctx->state, "sessions", s);
    }
    return s;
}

/* Resolve the `current` record. When args.current_sid names a registry record,
 * current IS that node (identity matches iff sid == current_sid). Otherwise
 * current is the standalone args.current dict (never identity-matches).
 * Returns a BORROWED pointer; *in_registry tells which case holds. */
static json_t *tmst_current(tmst_ctx_t *ctx, const json_t *args, bool *in_registry) {
    const char *csid = json_get_str(args, "current_sid", NULL);
    json_t *sessions = tmst_sessions(ctx);
    if (csid && csid[0]) {
        json_t *rec = json_obj_get(sessions, csid);
        if (rec && rec->type == JSON_OBJECT) { *in_registry = true; return rec; }
    }
    *in_registry = false;
    return json_obj_get(args, "current");
}

/* `_sessions.get(sid) is current` — pointer identity over the registry. */
static bool tmst_is_current(tmst_ctx_t *ctx, const char *sid, json_t *current,
                            bool in_registry) {
    if (!in_registry || !current) return false;
    json_t *sessions = tmst_sessions(ctx);
    return json_obj_get(sessions, sid) == current;
}

/* Propagate a seam raise unless it is an Exception caught by a suppress site.
 * Returns true when the caller should RETURN the (borrowed) raise node. */
static bool tmst_propagate(const json_t *r) {
    return tmst_is_raise(r);
}

/* ── durable identity helpers (_durable_ids) ── */
#define TMST_MESSAGE_UID "message_uid"
#define TMST_ABSORBED_UIDS "_absorbed_message_uids"

/* Build the sorted set-of-tuples projection of _durable_ids(msg) as a JSON
 * array of [tag, id] pairs (tag "row" sorts before "uid"; row ids numeric,
 * uid ids lexical). Returns a fresh owned array (empty for non-dict msg). */
static json_t *tmst_durable_ids(const json_t *msg) {
    json_t *out = json_array();
    if (!msg || msg->type != JSON_OBJECT) return out;
    /* collect (tag,id) candidates */
    json_t *tmp = json_array();
    const json_t *uid = json_obj_get(msg, TMST_MESSAGE_UID);
    if (uid && uid->type == JSON_STRING && uid->str_val && uid->str_val[0]) {
        json_t *p = json_array();
        json_append(p, json_string("uid"));
        json_append(p, json_copy(uid));
        json_append(tmp, p);
    }
    const json_t *absorbed = json_obj_get(msg, TMST_ABSORBED_UIDS);
    if (absorbed && absorbed->type == JSON_ARRAY) {
        for (size_t i = 0; i < absorbed->c.count; i++) {
            const json_t *u = absorbed->c.items[i];
            if (u && u->type == JSON_STRING && u->str_val && u->str_val[0]) {
                json_t *p = json_array();
                json_append(p, json_string("uid"));
                json_append(p, json_copy(u));
                json_append(tmp, p);
            }
        }
    }
    const json_t *row = json_obj_get(msg, "_row_id");
    /* isinstance(row_id, int) and not isinstance(row_id, bool): a JSON_NUMBER
     * that is integral and not a bool. */
    if (row && row->type == JSON_NUMBER &&
        row->num_val == (double)(long long)row->num_val) {
        json_t *p = json_array();
        json_append(p, json_string("row"));
        json_append(p, json_copy(row));
        json_append(tmp, p);
    }
    /* dedupe (set semantics) then sort by (tag, id) */
    for (size_t i = 0; i < tmp->c.count; i++) {
        json_t *p = tmp->c.items[i];
        bool dup = false;
        for (size_t j = 0; j < out->c.count; j++)
            if (tmst_json_eq(p, out->c.items[j])) { dup = true; break; }
        if (!dup) json_append(out, json_copy(p));
    }
    json_free(tmp);
    /* insertion sort (small n) */
    for (size_t i = 1; i < out->c.count; i++) {
        json_t *key = out->c.items[i];
        size_t j = i;
        while (j > 0) {
            json_t *prev = out->c.items[j - 1];
            const char *ptag = prev->c.items[0]->str_val;
            const char *ktag = key->c.items[0]->str_val;
            int c = strcmp(ptag, ktag);
            if (c == 0) {
                const json_t *pid = prev->c.items[1];
                const json_t *kid = key->c.items[1];
                if (!strcmp(ptag, "row"))
                    c = (pid->num_val < kid->num_val) ? -1 :
                        (pid->num_val > kid->num_val) ? 1 : 0;
                else
                    c = strcmp(pid->str_val ? pid->str_val : "",
                               kid->str_val ? kid->str_val : "");
            }
            if (c <= 0) break;
            out->c.items[j] = out->c.items[j - 1];
            j--;
        }
        out->c.items[j] = key;
    }
    return out;
}

/* set intersection non-empty? */
static bool tmst_ids_intersect(const json_t *a, const json_t *b) {
    if (!a || !b) return false;
    for (size_t i = 0; i < a->c.count; i++)
        for (size_t j = 0; j < b->c.count; j++)
            if (tmst_json_eq(a->c.items[i], b->c.items[j])) return true;
    return false;
}

/* ── pure leaves ── */

/* PoP: tmst__is_server_minted_key @ tui_gateway/session_registry.py:_is_server_minted_key */
static json_t *tmst__is_server_minted_key(tmst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *value = json_obj_get(args, "value");
    if (!tmst_truthy(value)) return json_bool(false);
    const char *s = tmst_str(value);
    if (!s || strlen(s) != 22) return json_bool(false);
    for (int i = 0; i < 22; i++) {
        char c = s[i];
        if (i == 8 || i == 15) {
            if (c != '_') return json_bool(false);
        } else if (i < 8 || (i >= 9 && i <= 14)) {
            if (c < '0' || c > '9') return json_bool(false);
        } else {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                return json_bool(false);
        }
    }
    return json_bool(true);
}

/* PoP: tmst__live_profile_matches @ tui_gateway/session_registry.py:_live_profile_matches */
static json_t *tmst__live_profile_matches(tmst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *session = json_obj_get(args, "session");
    const json_t *ph = json_obj_get(args, "profile_home");
    const char *phs = tmst_str(ph);
    if (phs && !strcmp(phs, "__ANY_PROFILE__")) return json_bool(true);
    const json_t *sh = session ? json_obj_get(session, "profile_home") : NULL;
    bool sh_truthy = tmst_truthy(sh);
    char *ph_str = tmst_truthy(ph) ? tmst_pystr(ph) : NULL;
    bool eq;
    if (!sh_truthy && !ph_str) eq = true;          /* None == None */
    else if (!sh_truthy || !ph_str) eq = false;
    else {
        char *sh_str = tmst_pystr(sh);
        eq = !strcmp(sh_str, ph_str);
        free(sh_str);
    }
    free(ph_str);
    return json_bool(eq);
}

/* PoP: tmst__session_lookup_key @ tui_gateway/session_registry.py:_session_lookup_key */
static json_t *tmst__session_lookup_key(tmst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *session = json_obj_get(args, "session");
    const json_t *fallback = json_obj_get(args, "fallback");
    const json_t *agent = session ? json_obj_get(session, "agent") : NULL;
    const json_t *asid = (agent && agent->type == JSON_OBJECT)
                             ? json_obj_get(agent, "session_id") : NULL;
    const json_t *skey = session ? json_obj_get(session, "session_key") : NULL;
    const json_t *val = tmst_truthy(asid) ? asid
                        : tmst_truthy(skey) ? skey
                        : tmst_truthy(fallback) ? fallback : NULL;
    char *s = val ? tmst_pystr(val) : tmst_strdup("");
    json_t *ret = json_string(s);
    free(s);
    return ret;
}

/* PoP: tmst__durable_ids @ tui_gateway/session_registry.py:_durable_ids */
static json_t *tmst__durable_ids(tmst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return tmst_durable_ids(json_obj_get(args, "msg"));
}

/* PoP: tmst__legacy_display_anchor @ tui_gateway/session_registry.py:_legacy_display_anchor */
static json_t *tmst__legacy_display_anchor(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *display = json_obj_get(args, "display");
    const json_t *memory = json_obj_get(args, "memory");
    json_t *prefix = json_array();
    if (display && display->type == JSON_ARRAY) {
        for (size_t i = 0; i < display->c.count; i++) {
            const json_t *msg = display->c.items[i];
            const json_t *role = msg ? json_obj_get(msg, "role") : NULL;
            const json_t *content = msg ? json_obj_get(msg, "content") : NULL;
            json_t *a = tmst_args0();
            tmst_aset(a, "content", content);
            json_t *text = tmst_call(ctx, "_coerce_message_text", a);
            if (tmst_propagate(text)) { json_free(prefix); return text; }
            json_t *pair = json_array();
            json_append(pair, role ? json_copy(role) : json_null());
            json_append(pair, text);
            json_append(prefix, pair);
        }
    }
    size_t cursor = 0, plen = prefix->c.count;
    long result = -1;
    if (memory && memory->type == JSON_ARRAY) {
        for (size_t index = 0; index < memory->c.count; index++) {
            const json_t *msg = memory->c.items[index];
            if (!msg || msg->type != JSON_OBJECT) continue;
            const json_t *role = json_obj_get(msg, "role");
            const json_t *content = json_obj_get(msg, "content");
            json_t *a = tmst_args0();
            tmst_aset(a, "content", content);
            json_t *text = tmst_call(ctx, "_coerce_message_text", a);
            if (tmst_propagate(text)) { json_free(prefix); return text; }
            json_t *value = json_array();
            json_append(value, role ? json_copy(role) : json_null());
            json_append(value, text);
            long found = -1;
            for (size_t k = cursor; k < plen; k++)
                if (tmst_json_eq(value, prefix->c.items[k])) { found = (long)k; break; }
            json_free(value);
            if (found < 0) continue;
            cursor = (size_t)found + 1;
            if (cursor == plen) { result = (long)index; break; }
        }
    }
    json_free(prefix);
    return json_number((double)result);
}

/* PoP: tmst__main_runtime_from_agent @ tui_gateway/session_registry.py:_main_runtime_from_agent */
static json_t *tmst__main_runtime_from_agent(tmst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *agent = json_obj_get(args, "agent");
    if (!agent || agent->type == JSON_NULL) return json_null();
    json_t *runtime = json_object();
    static const char *FIELDS[] = {
        "provider", "model", "base_url", "api_key", "api_mode", "auth_mode", "session_id"
    };
    for (size_t i = 0; i < 7; i++) {
        const char *field = FIELDS[i];
        const json_t *value = (agent->type == JSON_OBJECT) ? json_obj_get(agent, field) : NULL;
        const char *vs = tmst_str(value);
        if (vs) {
            char *stripped = tmst_strip(vs);
            if (stripped && stripped[0]) json_set(runtime, field, json_string(stripped));
            free(stripped);
        } else if (!strcmp(field, "api_key") && value && value->type == JSON_OBJECT &&
                   json_get_bool(value, "__callable__", false)) {
            json_set(runtime, field, json_copy(value));
        }
    }
    if (runtime->c.count == 0) { json_free(runtime); return json_null(); }
    return runtime;
}

/* PoP: tmst__new_session_key @ tui_gateway/session_registry.py:_new_session_key */
static json_t *tmst__new_session_key(tmst_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmst_call(ctx, "new_session_id", tmst_args0());
}

/* PoP: tmst__session_pending_kind @ tui_gateway/session_registry.py:_session_pending_kind */
static json_t *tmst__session_pending_kind(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    json_t *a = tmst_args0();
    tmst_aset(a, "sid", sid);
    return tmst_call(ctx, "pending_kind", a);
}

/* ── status core + reconcile ── */

/* PoP: tmst__session_live_status @ tui_gateway/session_registry.py:_session_live_status */
static json_t *tmst__session_live_status(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    const json_t *session = json_obj_get(args, "session");
    json_t *a = tmst_args0();
    tmst_aset(a, "sid", sid);
    json_t *kind = tmst_call(ctx, "_session_pending_kind", a);
    if (tmst_propagate(kind)) return kind;
    if (tmst_truthy(kind)) { json_free(kind); return json_string("waiting"); }
    json_free(kind);
    const json_t *ready = session ? json_obj_get(session, "agent_ready") : NULL;
    if (ready && ready->type != JSON_NULL) {
        json_t *isset = tmst_call(ctx, "agent_ready.is_set", tmst_args0());
        if (tmst_propagate(isset)) return isset;
        bool set = tmst_truthy(isset);
        json_free(isset);
        const json_t *started = session ? json_obj_get(session, "agent_build_started") : NULL;
        if (!set && tmst_truthy(started)) return json_string("starting");
    }
    const json_t *running = session ? json_obj_get(session, "running") : NULL;
    return json_string(tmst_truthy(running) ? "working" : "idle");
}

/* PoP: tmst__session_live_title @ tui_gateway/session_registry.py:_session_live_title */
static json_t *tmst__session_live_title(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const json_t *key = json_obj_get(args, "key");
    const json_t *pt = session ? json_obj_get(session, "pending_title") : NULL;
    char *init = tmst_truthy(pt) ? tmst_pystr(pt) : tmst_strdup("");
    char *title = tmst_strip(init);
    free(init);
    json_t *ret;
    /* with contextlib.suppress(Exception), _session_db(session) as db, ... */
    json_t *a = tmst_args0();
    tmst_aset(a, "session", session);
    json_t *db = tmst_call(ctx, "session_db", a);
    if (tmst_is_raise(db)) {
        if (tmst_is_base_exc(tmst_raise_kind(db))) { free(title); return db; }
        json_free(db);
        ret = json_string(title); free(title); return ret;
    }
    json_t *a2 = tmst_args0();
    tmst_asets(a2, "db", (db && db->type != JSON_NULL) ? "db" : NULL);
    json_t *persistence = tmst_call(ctx, "persistence_for", a2);
    if (tmst_is_raise(persistence)) {
        json_free(db);
        if (tmst_is_base_exc(tmst_raise_kind(persistence))) { free(title); return persistence; }
        json_free(persistence);
        ret = json_string(title); free(title); return ret;
    }
    if (persistence && persistence->type != JSON_NULL) {
        json_t *a3 = tmst_args0();
        tmst_aset(a3, "key", key);
        json_t *gt = tmst_call(ctx, "get_title", a3);
        if (tmst_is_raise(gt)) {
            json_free(db); json_free(persistence);
            if (tmst_is_base_exc(tmst_raise_kind(gt))) { free(title); return gt; }
            json_free(gt);
            ret = json_string(title); free(title); return ret;
        }
        const char *pick = tmst_truthy(gt) ? tmst_str(gt) : (title[0] ? title : "");
        char *nt = tmst_strip(pick ? pick : "");
        free(title);
        title = nt;
        json_free(gt);
    }
    json_free(db);
    json_free(persistence);
    ret = json_string(title);
    free(title);
    return ret;
}

/* PoP: tmst__reconcile_display_with_live @ tui_gateway/session_registry.py:_reconcile_display_with_live */
static json_t *tmst__reconcile_display_with_live(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *db_display = json_obj_get(args, "db_display");
    const json_t *in_memory = json_obj_get(args, "in_memory");
    if (!tmst_truthy(db_display)) return in_memory ? json_copy(in_memory) : json_null();
    if (!tmst_truthy(in_memory)) return json_copy(db_display);
    json_t *last = json_get(db_display, json_len(db_display) - 1);
    json_t *anchor = tmst_durable_ids(last);
    long last_shared = -1;
    for (size_t idx = 0; idx < in_memory->c.count; idx++) {
        json_t *ids = tmst_durable_ids(in_memory->c.items[idx]);
        if (tmst_ids_intersect(ids, anchor)) last_shared = (long)idx;
        json_free(ids);
    }
    if (last_shared == -1) {
        bool any_ids = false;
        if (anchor->c.count > 0) {
            for (size_t i = 0; i < in_memory->c.count; i++) {
                json_t *ids = tmst_durable_ids(in_memory->c.items[i]);
                if (ids->c.count > 0) { any_ids = true; json_free(ids); break; }
                json_free(ids);
            }
        }
        if (anchor->c.count > 0 && any_ids) { json_free(anchor); return json_copy(db_display); }
        json_t *a = tmst_args0();
        tmst_aset(a, "display", db_display);
        tmst_aset(a, "memory", in_memory);
        json_t *ar = tmst__legacy_display_anchor(ctx, a);
        json_free(a);
        if (tmst_is_raise(ar)) { json_free(anchor); return ar; }
        last_shared = (long)ar->num_val;
        json_free(ar);
    }
    json_free(anchor);
    if (last_shared == -1) return json_copy(db_display);
    json_t *out = json_copy(db_display);
    for (size_t i = (size_t)last_shared + 1; i < in_memory->c.count; i++)
        json_append(out, json_copy(in_memory->c.items[i]));
    return out;
}

/* ── registry iteration ── */

/* PoP: tmst__any_live_session_claims_key @ tui_gateway/session_registry.py:_any_live_session_claims_key */
static json_t *tmst__any_live_session_claims_key(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *target = json_obj_get(args, "target");
    char *t = tmst_pystr(target);
    json_t *sessions = tmst_sessions(ctx);
    bool found = false;
    for (size_t i = 0; i < sessions->c.count; i++) {
        json_t *rec = sessions->c.items[i];
        if (!rec || rec->type != JSON_OBJECT) continue;
        const json_t *sk = json_obj_get(rec, "session_key");
        char *s = tmst_truthy(sk) ? tmst_pystr(sk) : tmst_strdup("");
        if (!strcmp(s, t)) { found = true; free(s); break; }
        free(s);
    }
    free(t);
    return json_bool(found);
}

/* PoP: tmst__find_live_session_by_key @ tui_gateway/session_registry.py:_find_live_session_by_key */
static json_t *tmst__find_live_session_by_key(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *session_key = json_obj_get(args, "session_key");
    const json_t *ph = json_obj_get(args, "profile_home");
    json_t *ph_default = NULL;
    if (!ph) { ph_default = json_string("__ANY_PROFILE__"); ph = ph_default; }
    json_t *sessions = tmst_sessions(ctx);
    json_t *ret = json_null();
    for (size_t i = 0; i < sessions->c.count; i++) {
        const char *sid = sessions->c.keys[i];
        json_t *session = sessions->c.items[i];
        if (!session || session->type != JSON_OBJECT) continue;
        const json_t *fin = json_obj_get(session, "_finalized");
        if (tmst_truthy(fin)) continue;
        json_t *a = tmst_args0();
        tmst_aset(a, "session", session);
        tmst_asets(a, "fallback", sid);
        json_t *lk = tmst__session_lookup_key(ctx, a);
        json_free(a);
        bool key_match = tmst_json_eq(lk, session_key);
        json_free(lk);
        if (!key_match) continue;
        json_t *a2 = tmst_args0();
        tmst_aset(a2, "session", session);
        tmst_aset(a2, "profile_home", ph);
        json_t *pm = tmst__live_profile_matches(ctx, a2);
        json_free(a2);
        bool prof_match = tmst_truthy(pm);
        json_free(pm);
        if (!prof_match) continue;
        json_free(ret);
        ret = json_array();
        json_append(ret, json_string(sid));
        json_append(ret, json_copy(session));
        break;
    }
    json_free(ph_default);
    return ret;
}

/* ── registry mutation flows ── */

/* PoP: tmst__claim_parked_runtimes @ tui_gateway/session_registry.py:_claim_parked_runtimes */
static json_t *tmst__claim_parked_runtimes(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *session_key = json_obj_get(args, "session_key");
    const json_t *keep_sid = json_obj_get(args, "keep_sid");
    const char *ks = tmst_str(keep_sid) ? tmst_str(keep_sid) : "";
    const json_t *ph = json_obj_get(args, "profile_home");
    json_t *ph_default = NULL;
    if (!ph) { ph_default = json_string("__ANY_PROFILE__"); ph = ph_default; }
    json_t *sessions = tmst_sessions(ctx);
    json_t *cand = json_array();
    for (size_t i = 0; i < sessions->c.count; i++) {
        const char *old_sid = sessions->c.keys[i];
        json_t *old = sessions->c.items[i];
        if (!old || old->type != JSON_OBJECT) continue;
        if (!strcmp(old_sid, ks)) continue;
        const json_t *fin = json_obj_get(old, "_finalized");
        if (tmst_truthy(fin)) continue;
        json_t *a = tmst_args0();
        tmst_aset(a, "session", old);
        tmst_asets(a, "fallback", old_sid);
        json_t *lk = tmst__session_lookup_key(ctx, a);
        json_free(a);
        bool km = tmst_json_eq(lk, session_key);
        json_free(lk);
        if (!km) continue;
        json_t *a2 = tmst_args0();
        tmst_aset(a2, "session", old);
        tmst_aset(a2, "profile_home", ph);
        json_t *pm = tmst__live_profile_matches(ctx, a2);
        json_free(a2);
        bool pmatch = tmst_truthy(pm);
        json_free(pm);
        if (!pmatch) continue;
        const json_t *tr = json_obj_get(old, "transport");
        const char *trs = tmst_str(tr);
        if (!(trs && !strcmp(trs, "__DETACHED_WS__"))) continue;
        json_append(cand, json_string(old_sid));
    }
    json_t *stale = json_array();
    for (size_t i = 0; i < cand->c.count; i++) {
        const char *old_sid = cand->c.items[i]->str_val;
        json_t *a = tmst_args0();
        tmst_asets(a, "sid", old_sid);
        json_t *r = tmst_call(ctx, "_cancel_ws_orphan_reap", a);
        if (tmst_propagate(r)) { json_free(cand); json_free(stale); json_free(ph_default); return r; }
        json_free(r);
        json_t *a2 = tmst_args0();
        tmst_asets(a2, "sid", old_sid);
        json_t *popped = tmst_call(ctx, "_pop_session_by_id", a2);
        if (tmst_propagate(popped)) { json_free(cand); json_free(stale); json_free(ph_default); return popped; }
        if (popped && popped->type != JSON_NULL) {
            json_obj_del(sessions, old_sid);
            json_t *pair = json_array();
            json_append(pair, json_string(old_sid));
            json_append(pair, popped);
            json_append(stale, pair);
        } else {
            json_free(popped);
        }
    }
    json_free(cand);
    json_free(ph_default);
    return stale;
}

/* PoP: tmst__finalize_superseded_runtimes @ tui_gateway/session_registry.py:_finalize_superseded_runtimes */
static json_t *tmst__finalize_superseded_runtimes(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *stale = json_obj_get(args, "stale");
    if (stale && stale->type == JSON_ARRAY) {
        for (size_t i = 0; i < stale->c.count; i++) {
            json_t *pair = stale->c.items[i];
            const json_t *old_sid = json_get(pair, 0);
            const json_t *popped = json_get(pair, 1);
            json_t *a = tmst_args0();
            tmst_aset(a, "popped", popped);
            tmst_asets(a, "end_reason", "superseded_by_resume");
            json_t *r = tmst_call(ctx, "_teardown_popped_session", a);
            if (tmst_is_raise(r)) {
                if (tmst_is_base_exc(tmst_raise_kind(r))) return r;
                json_free(r);
                json_t *la = tmst_args0();
                tmst_asets(la, "fmt", "superseded runtime teardown failed sid=%s");
                json_t *largs = json_array();
                json_append(largs, old_sid ? json_copy(old_sid) : json_null());
                json_set(la, "args", largs);
                json_t *lr = tmst_call(ctx, "log.exception", la);
                if (tmst_propagate(lr)) return lr;
                json_free(lr);
            } else {
                json_free(r);
            }
        }
    }
    return json_null();
}

/* PoP: tmst__claim_or_reuse_live @ tui_gateway/session_registry.py:_claim_or_reuse_live */
static json_t *tmst__claim_or_reuse_live(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    const json_t *session_key = json_obj_get(args, "session_key");
    const json_t *record = json_obj_get(args, "record");
    const json_t *lease = json_obj_get(args, "lease");
    const json_t *ph = record ? json_obj_get(record, "profile_home") : NULL;
    const char *sids = tmst_str(sid) ? tmst_str(sid) : "";
    json_t *a = tmst_args0();
    tmst_aset(a, "session_key", session_key);
    tmst_aset(a, "profile_home", ph);
    json_t *live = tmst_call(ctx, "_find_live_session_by_key", a);
    if (tmst_propagate(live)) return live;
    if (live && live->type != JSON_NULL) {
        if (lease && lease->type != JSON_NULL) {
            json_t *r = tmst_call(ctx, "lease.release", tmst_args0());
            if (tmst_propagate(r)) { json_free(live); return r; }
            json_free(r);
        }
        return live;
    }
    json_free(live);
    json_t *sessions = tmst_sessions(ctx);
    json_set(sessions, sids, json_copy(record));
    json_t *registered = json_obj_get(sessions, sids);
    json_t *a2 = tmst_args0();
    tmst_aset(a2, "record", registered);
    json_t *r2 = tmst_call(ctx, "_register_session_cwd", a2);
    if (tmst_propagate(r2)) return r2;
    json_free(r2);
    json_t *a3 = tmst_args0();
    tmst_aset(a3, "sid", sid);
    json_t *r3 = tmst_call(ctx, "_cancel_ws_orphan_reap", a3);
    if (tmst_propagate(r3)) return r3;
    json_free(r3);
    json_t *a4 = tmst_args0();
    tmst_aset(a4, "session_key", session_key);
    tmst_aset(a4, "keep_sid", sid);
    tmst_aset(a4, "profile_home", ph);
    json_t *stale = tmst_call(ctx, "_claim_parked_runtimes", a4);
    if (tmst_propagate(stale)) return stale;
    json_t *a5 = tmst_args0();
    tmst_aset(a5, "stale", stale);
    json_t *r5 = tmst_call(ctx, "_finalize_superseded_runtimes", a5);
    json_free(stale);
    if (tmst_propagate(r5)) return r5;
    json_free(r5);
    return json_null();
}

/* PoP: tmst__start_session_services @ tui_gateway/session_registry.py:_start_session_services */
static json_t *tmst__start_session_services(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *current = json_obj_get(args, "current");
    json_t *sessions = tmst_sessions(ctx);
    const char *sids = tmst_str(sid) ? tmst_str(sid) : "";
    json_t *rec = json_obj_get(sessions, sids);
    if (rec && rec->type != JSON_NULL) {
        const json_t *old_stop = json_obj_get(rec, "_notif_stop");
        if (old_stop && old_stop->type != JSON_NULL) {
            json_t *r = tmst_call(ctx, "old_stop.set", tmst_args0());
            if (tmst_propagate(r)) return r;
            json_free(r);
        }
        json_t *a = tmst_args0();
        tmst_aset(a, "sid", sid);
        tmst_aset(a, "rec", rec);
        json_t *ns = tmst_call(ctx, "_start_notification_poller", a);
        if (tmst_propagate(ns)) return ns;
        json_set(rec, "_notif_stop", ns);
    }
    json_t *a2 = tmst_args0();
    tmst_aset(a2, "session", current);
    json_t *src = tmst_call(ctx, "_session_source", a2);
    if (tmst_propagate(src)) return src;
    json_t *a3 = tmst_args0();
    tmst_asets(a3, "hook", "on_session_reset");
    tmst_aset(a3, "key", key);
    tmst_aset(a3, "source", src);
    json_t *r3 = tmst_call(ctx, "_notify_session_boundary", a3);
    json_free(src);
    if (tmst_propagate(r3)) return r3;
    json_free(r3);
    return json_null();
}

/* ── build / wait flows ── */

#define TMST_AGENT_STILL_STARTING \
    "Hermes is still starting this session (loading tools), so this command could not run yet. " \
    "Wait for the status bar to show ready and try again."
#define TMST_SESS_NOWAIT_FMT \
    "session-scoped RPC rejected: method=%s session_id=%r not in memory " \
    "(detached/reaped runtime; client should resume the stored session), rid=%r"

/* PoP: tmst__wait_agent @ tui_gateway/session_registry.py:_wait_agent */
static json_t *tmst__wait_agent(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const json_t *rid = json_obj_get(args, "rid");
    double timeout = json_get_num(args, "timeout", 30.0);
    const json_t *ready = session ? json_obj_get(session, "agent_ready") : NULL;
    if (ready && ready->type != JSON_NULL) {
        json_t *a = tmst_args0();
        tmst_asetn(a, "timeout", timeout);
        json_t *w = tmst_call(ctx, "agent_ready.wait", a);
        if (tmst_propagate(w)) return w;
        bool ok = tmst_truthy(w);
        json_free(w);
        if (!ok) {
            json_t *ea = tmst_args0();
            tmst_aset(ea, "rid", rid);
            tmst_asetn(ea, "code", 5032);
            tmst_asets(ea, "message", TMST_AGENT_STILL_STARTING);
            return tmst_call(ctx, "_err", ea);
        }
    }
    const json_t *err = session ? json_obj_get(session, "agent_error") : NULL;
    if (tmst_truthy(err)) {
        json_t *ea = tmst_args0();
        tmst_aset(ea, "rid", rid);
        tmst_asetn(ea, "code", 5032);
        tmst_aset(ea, "message", err);
        return tmst_call(ctx, "_err", ea);
    }
    return json_null();
}

/* PoP: tmst__agent_build_wait_cap @ tui_gateway/session_registry.py:_agent_build_wait_cap */
static json_t *tmst__agent_build_wait_cap(tmst_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cfg = tmst_call(ctx, "_load_cfg", tmst_args0());
    if (tmst_is_raise(cfg)) {
        if (tmst_is_base_exc(tmst_raise_kind(cfg))) return cfg;
        json_free(cfg);
        return json_number(600.0);
    }
    if (!cfg || cfg->type != JSON_OBJECT) { json_free(cfg); return json_number(600.0); }
    const json_t *agent = json_obj_get(cfg, "agent");
    const json_t *src = tmst_truthy(agent) ? agent : NULL;
    if (src && src->type != JSON_OBJECT) { json_free(cfg); return json_number(600.0); }
    const json_t *raw = src ? json_obj_get(src, "build_wait_timeout") : NULL;
    double result = 600.0;
    if (raw && raw->type != JSON_NULL) {
        bool ok;
        double f = tmst_py_float(raw, &ok);
        if (!ok) { json_free(cfg); return json_number(600.0); }
        if (f > 0) result = f;
    }
    json_free(cfg);
    return json_number(result);
}

/* PoP: tmst__await_resume_history @ tui_gateway/session_registry.py:_await_resume_history */
static json_t *tmst__await_resume_history(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    bool in_reg;
    json_t *current = tmst_current(ctx, args, &in_reg);
    const json_t *hr = current ? json_obj_get(current, "resume_history_ready") : NULL;
    if (!hr || hr->type == JSON_NULL) return json_bool(true);
    json_t *a = tmst_args0();
    tmst_asetn(a, "timeout", 300.0);
    json_t *w = tmst_call(ctx, "resume_history.wait", a);
    if (tmst_propagate(w)) return w;
    bool ok = tmst_truthy(w);
    json_free(w);
    if (!ok) return tmst_err_spec("TimeoutError", "session history hydration timed out");
    const json_t *herr = current ? json_obj_get(current, "resume_history_error") : NULL;
    if (tmst_truthy(herr)) {
        char *s = tmst_pystr(herr);
        json_t *e = tmst_err_spec("RuntimeError", s);
        free(s);
        return e;
    }
    return json_bool(tmst_is_current(ctx, tmst_str(sid) ? tmst_str(sid) : "", current, in_reg));
}

/* PoP: tmst__attach_built_agent @ tui_gateway/session_registry.py:_attach_built_agent */
static json_t *tmst__attach_built_agent(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    bool in_reg;
    json_t *current = tmst_current(ctx, args, &in_reg);
    const json_t *agent_in = json_obj_get(args, "agent");
    json_t *agent = (agent_in && agent_in->type == JSON_OBJECT) ? json_copy(agent_in) : json_null();
    const json_t *pt = current ? json_obj_get(current, "pending_title") : NULL;
    char *init = tmst_truthy(pt) ? tmst_pystr(pt) : tmst_strdup("");
    char *hint = tmst_strip(init);
    free(init);
    if (hint[0] && agent->type == JSON_OBJECT)
        json_set_hdr(agent, "_session_title_hint", hint);
    free(hint);
    if (!tmst_is_current(ctx, tmst_str(sid) ? tmst_str(sid) : "", current, in_reg)) {
        json_free(agent);
        return json_bool(false);
    }
    json_set(current, "agent", json_copy(agent));
    json_t *a = tmst_args0();
    tmst_aset(a, "record", current);
    json_t *r = tmst_call(ctx, "_register_session_cwd", a);
    if (tmst_propagate(r)) { json_free(agent); return r; }
    json_free(r);
    json_t *a2 = tmst_args0();
    tmst_aset(a2, "record", current);
    json_t *r2 = tmst_call(ctx, "_session_todo_state", a2);
    if (tmst_propagate(r2)) { json_free(agent); return r2; }
    json_free(r2);
    json_t *cm = tmst_call(ctx, "_config_model_target", tmst_args0());
    if (tmst_propagate(cm)) { json_free(agent); return cm; }
    json_set(current, "config_model_seen", cm);
    json_free(agent);
    return json_bool(true);
}

/* PoP: tmst__finish_agent_build @ tui_gateway/session_registry.py:_finish_agent_build */
static json_t *tmst__finish_agent_build(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    const json_t *key = json_obj_get(args, "key");
    bool notify_registered = json_get_bool(args, "notify_registered", false);
    const json_t *scopes = json_obj_get(args, "scopes");
    const json_t *session_db = json_obj_get(args, "session_db");
    const json_t *persistence = json_obj_get(args, "persistence");
    bool in_reg;
    json_t *current = tmst_current(ctx, args, &in_reg);
    if (scopes && scopes->type != JSON_NULL) {
        json_t *a = tmst_args0();
        tmst_aset(a, "scopes", scopes);
        json_t *r = tmst_call(ctx, "_release_build_profile_scopes", a);
        if (tmst_propagate(r)) return r;
        json_free(r);
    }
    bool replaced = !tmst_is_current(ctx, tmst_str(sid) ? tmst_str(sid) : "", current, in_reg);
    if (replaced && notify_registered) {
        json_t *a = tmst_args0();
        tmst_aset(a, "key", key);
        json_t *r = tmst_call(ctx, "unregister_gateway_notify", a);
        if (tmst_is_raise(r)) {
            if (tmst_is_base_exc(tmst_raise_kind(r))) return r;
            json_free(r);
        } else {
            json_free(r);
        }
    }
    if (session_db && session_db->type != JSON_NULL) {
        const json_t *agent = (!replaced && current) ? json_obj_get(current, "agent") : NULL;
        json_t *a = tmst_args0();
        tmst_asets(a, "agent", (agent && agent->type != JSON_NULL) ? "agent" : NULL);
        tmst_asets(a, "db", "db");
        json_t *tr = tmst_call(ctx, "_transfer_db_to_agent", a);
        if (tmst_propagate(tr)) return tr;
        bool transferred = tmst_truthy(tr);
        json_free(tr);
        if (!transferred) {
            json_t *r = tmst_call(ctx, "db.close", tmst_args0());
            if (tmst_is_raise(r)) {
                if (tmst_is_base_exc(tmst_raise_kind(r))) return r;
                json_free(r);
            } else {
                json_free(r);
            }
            json_t *a2 = tmst_args0();
            tmst_asets(a2, "persistence",
                       (persistence && persistence->type != JSON_NULL) ? "persistence" : NULL);
            json_t *r2 = tmst_call(ctx, "_release_persistence", a2);
            if (tmst_propagate(r2)) return r2;
            json_free(r2);
        }
    }
    return json_null();
}

/* ── scope / kwargs flows ── */

/* PoP: tmst__bind_build_profile_scopes @ tui_gateway/session_registry.py:_bind_build_profile_scopes */
static json_t *tmst__bind_build_profile_scopes(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *ph = json_obj_get(args, "profile_home");
    json_t *a = tmst_args0();
    tmst_aset(a, "profile_home", ph);
    json_t *r = tmst_call(ctx, "_profile_runtime_scope_tokens", a);
    if (tmst_is_raise(r)) {
        if (tmst_is_base_exc(tmst_raise_kind(r))) return r;
        json_free(r);
    } else {
        return r;
    }
    if (!tmst_truthy(ph)) return json_null();
    json_t *a2 = tmst_args0();
    tmst_aset(a2, "profile_home", ph);
    json_t *scope = tmst_call(ctx, "scope_for_profile", a2);
    if (tmst_is_raise(scope)) {
        if (tmst_is_base_exc(tmst_raise_kind(scope))) return scope;
        json_free(scope);
        return json_null();
    }
    json_t *a3 = tmst_args0();
    tmst_asets(a3, "scope", "scope");
    json_set(a3, "secrets", json_object());
    json_t *token = tmst_call(ctx, "bind_state", a3);
    json_free(scope);
    if (tmst_is_raise(token)) {
        if (tmst_is_base_exc(tmst_raise_kind(token))) return token;
        json_free(token);
        return json_null();
    }
    return token;
}

/* PoP: tmst__release_build_profile_scopes @ tui_gateway/session_registry.py:_release_build_profile_scopes */
static json_t *tmst__release_build_profile_scopes(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *scopes = json_obj_get(args, "scopes");
    json_t *a = tmst_args0();
    tmst_aset(a, "scopes", scopes);
    json_t *r = tmst_call(ctx, "_release_profile_runtime_scope_tokens", a);
    if (tmst_is_raise(r)) {
        if (tmst_is_base_exc(tmst_raise_kind(r))) return r;
        json_free(r);
    } else {
        json_free(r);
    }
    return json_null();
}

/* PoP: tmst__deferred_build_agent_kwargs @ tui_gateway/session_registry.py:_deferred_build_agent_kwargs */
static json_t *tmst__deferred_build_agent_kwargs(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *current = json_obj_get(args, "current");
    const json_t *session_db = json_obj_get(args, "session_db");
    json_t *kw = json_object();
    json_set(kw, "session_db", session_db ? json_copy(session_db) : json_null());
    json_t *a = tmst_args0();
    tmst_aset(a, "current", current);
    json_t *ca = tmst_call(ctx, "_context_cwd_is_launch_artifact", a);
    if (tmst_propagate(ca)) { json_free(kw); return ca; }
    json_set(kw, "context_cwd_is_launch_artifact", ca);
    json_t *a2 = tmst_args0();
    tmst_aset(a2, "session", current);
    json_t *src = tmst_call(ctx, "_session_source", a2);
    if (tmst_propagate(src)) { json_free(kw); return src; }
    json_set(kw, "platform_override", src);
    json_t *a3 = tmst_args0();
    tmst_aset(a3, "session", current);
    json_t *cwd = tmst_call(ctx, "_session_cwd", a3);
    if (tmst_propagate(cwd)) { json_free(kw); return cwd; }
    json_set(kw, "cwd_override", cwd);
    const json_t *rsid = current ? json_obj_get(current, "resume_session_id") : NULL;
    if (tmst_truthy(rsid)) json_set(kw, "session_id", json_copy(rsid));
    const json_t *ro = current ? json_obj_get(current, "resume_runtime_overrides") : NULL;
    bool use_ro = false;
    if (ro && ro->type == JSON_OBJECT && ro->c.count > 0) {
        json_t *a4 = tmst_args0();
        tmst_aset(a4, "overrides", ro);
        json_t *routable = tmst_call(ctx, "_overrides_have_routable_provider", a4);
        if (tmst_propagate(routable)) { json_free(kw); return routable; }
        use_ro = tmst_truthy(routable);
        json_free(routable);
    }
    if (use_ro) {
        for (size_t i = 0; i < ro->c.count; i++)
            json_set(kw, ro->c.keys[i], json_copy(ro->c.items[i]));
    } else {
        const json_t *mo = current ? json_obj_get(current, "model_override") : NULL;
        if (tmst_truthy(mo)) json_set(kw, "model_override", json_copy(mo));
        const json_t *rc = current ? json_obj_get(current, "create_reasoning_override") : NULL;
        if (rc && rc->type != JSON_NULL) json_set(kw, "reasoning_config_override", json_copy(rc));
        const json_t *st = current ? json_obj_get(current, "create_service_tier_override") : NULL;
        if (st && st->type != JSON_NULL) json_set(kw, "service_tier_override", json_copy(st));
    }
    return kw;
}

/* ── RPC session resolvers ── */

/* PoP: tmst__sess_nowait @ tui_gateway/session_registry.py:_sess_nowait */
static json_t *tmst__sess_nowait(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *psid = params ? json_obj_get(params, "session_id") : NULL;
    char *sid = tmst_truthy(psid) ? tmst_pystr(psid) : tmst_strdup("");
    json_t *sessions = tmst_sessions(ctx);
    json_t *s = json_obj_get(sessions, sid);
    if (tmst_truthy(s)) {
        json_t *ret = json_array();
        json_append(ret, json_copy(s));
        json_append(ret, json_null());
        free(sid);
        return ret;
    }
    json_t *m = tmst_call(ctx, "_current_rpc_method.get", tmst_args0());
    if (tmst_propagate(m)) { free(sid); return m; }
    json_t *method = tmst_truthy(m) ? m : json_string("?");
    if (method != m) json_free(m);
    json_t *la = tmst_args0();
    tmst_asets(la, "fmt", TMST_SESS_NOWAIT_FMT);
    json_t *largs = json_array();
    json_append(largs, method);
    json_append(largs, json_string(sid));
    json_append(largs, rid ? json_copy(rid) : json_null());
    json_set(la, "args", largs);
    json_t *lr = tmst_call(ctx, "log.warning", la);
    if (tmst_propagate(lr)) { free(sid); return lr; }
    json_free(lr);
    json_t *ea = tmst_args0();
    tmst_aset(ea, "rid", rid);
    tmst_asetn(ea, "code", 4001);
    tmst_asets(ea, "message", "session not found");
    json_t *err = tmst_call(ctx, "_err", ea);
    if (tmst_propagate(err)) { free(sid); return err; }
    json_t *ret = json_array();
    json_append(ret, json_null());
    json_append(ret, err);
    free(sid);
    return ret;
}

/* PoP: tmst__sess_building @ tui_gateway/session_registry.py:_sess_building */
static json_t *tmst__sess_building(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    const json_t *rid = json_obj_get(args, "rid");
    json_t *a = tmst_args0();
    tmst_aset(a, "params", params);
    tmst_aset(a, "rid", rid);
    json_t *se = tmst_call(ctx, "_sess_nowait", a);
    if (tmst_propagate(se)) return se;
    json_t *s = json_get(se, 0);
    json_t *err = json_get(se, 1);
    if (!tmst_truthy(err)) {
        const json_t *psid = params ? json_obj_get(params, "session_id") : NULL;
        char *sid = tmst_truthy(psid) ? tmst_pystr(psid) : tmst_strdup("");
        json_t *a2 = tmst_args0();
        tmst_asets(a2, "sid", sid);
        tmst_aset(a2, "session", s);
        free(sid);
        json_t *r = tmst_call(ctx, "_start_agent_build", a2);
        if (tmst_propagate(r)) { json_free(se); return r; }
        json_free(r);
    }
    json_t *ret = json_array();
    if (tmst_truthy(err)) {
        json_append(ret, json_null());
        json_append(ret, json_copy(err));
    } else {
        json_append(ret, s ? json_copy(s) : json_null());
        json_append(ret, json_null());
    }
    json_free(se);
    return ret;
}

/* PoP: tmst__sess @ tui_gateway/session_registry.py:_sess */
static json_t *tmst__sess(tmst_ctx_t *ctx, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    const json_t *rid = json_obj_get(args, "rid");
    json_t *a = tmst_args0();
    tmst_aset(a, "params", params);
    tmst_aset(a, "rid", rid);
    json_t *se = tmst_call(ctx, "_sess_building", a);
    if (tmst_propagate(se)) return se;
    json_t *s = json_get(se, 0);
    json_t *err = json_get(se, 1);
    if (tmst_truthy(err)) {
        json_t *ret = json_array();
        json_append(ret, json_null());
        json_append(ret, json_copy(err));
        json_free(se);
        return ret;
    }
    json_t *a2 = tmst_args0();
    tmst_aset(a2, "session", s);
    tmst_aset(a2, "rid", rid);
    tmst_asetn(a2, "timeout", 30.0);
    json_t *w = tmst_call(ctx, "_wait_agent", a2);
    if (tmst_propagate(w)) { json_free(se); return w; }
    json_t *ret = json_array();
    json_append(ret, s ? json_copy(s) : json_null());
    json_append(ret, w);
    json_free(se);
    return ret;
}