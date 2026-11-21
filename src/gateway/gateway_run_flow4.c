/*
 * gateway_run_flow4.c — gateway/run.py FLOW cores, fourth wave
 * (universal-seam convention, twin of gateway_run_flow2 / gateway_run_flow3).
 *
 * Ports 14 UNCLAIMED gateway/run.py GatewayRunner methods (none of these are
 * in gateway_run_flow/flow2/flow3): the status-action leaves
 * (_status_action_label / _status_action_gerund), the per-session map cluster
 * (_sessions_map / _session_state / _peek_session_state / _is_session_running /
 * _running_agent_items / _running_agent_count), the session-key resolver
 * (_resolve_session_key_or_none), the cached-source cluster
 * (_cache_session_source / _get_cached_session_source), the executor pool
 * helpers (_get_or_create_pool / _stop_pool), the runtime-status writer
 * (_update_runtime_status), and the session-db property getter (_session_db).
 *
 * Every live I/O enters through gr4_seams_t.call(ctx, name, args); args is
 * owned by gr4_call (freed after the borrowed call); results are NEW json_t
 * with NULL == Python None and {"__raise__":K,"msg":M} == a raised exception.
 * BaseException kinds (SystemExit/KeyboardInterrupt/CancelledError/
 * GeneratorExit) pass through every `except Exception` site. Fail-closed:
 * the harness delivers a RuntimeError raise spec for an unscripted seam.
 *
 * Seam name table (name -> live call):
 *   t                            t("gateway.busy.action_*")
 *   _sessions_map                claimed sibling self._sessions_map()
 *   SessionState                 SessionState() construction
 *   _peek_session_state          claimed sibling self._peek_session_state(key)
 *   _session_key_for_source      claimed sibling self._session_key_for_source(source)
 *   move_to_end                  cached_sources.move_to_end(session_key)
 *   Lock                         threading.Lock() construction
 *   lock.enter / lock.exit       `with lock:` enter / exit
 *   make_pool                    make_pool() factory
 *   shutdown                     executor.shutdown(wait=, cancel_futures=)
 *   _describe_active_work        claimed sibling self._describe_active_work()
 *   _active_work_count           claimed sibling self._active_work_count()
 *   _write_runtime_status_quiet  module-level collaborator(**fields)
 *   _open_session_db_for_active_scope  claimed sibling (property getter)
 *
 * Modeling notes (differential oracle tests/oracle_gateway_run_flow4.py):
 *  - Runner DATA attributes live in in.state; METHOD-presence markers (for
 *    seamed siblings) live in in.runner. A direct `self.X` read of an absent
 *    key yields the live-shaped AttributeError; a `getattr(self,X,default)`
 *    read defaults to None via the double's __getattr__.
 *  - Seam results are deep-copied by the oracle (json_copy on the C side):
 *    flows that mutate a returned map still leave the recorded trace intact.
 *  - `_session_db` sentinel _SESSION_DB_UNPINNED travels as
 *    {"__unpinned__": true}; a pinned value is echoed verbatim.
 *  - Result sets/lists use insertion order; no set-valued returns here.
 *  - str()/truthiness positions accept the scalar domain; integral floats
 *    serialize int-like and normalize on both sides.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "libjson/json.h"

typedef struct gr4_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gr4_seams_t;

/* ── universal-seam helpers ── */
static json_t *gr4_call(const gr4_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool gr4_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *gr4_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static json_t *gr4_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool gr4_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static bool gr4_truth(const json_t *v) {
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
static const json_t *gr4_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *gr4_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static json_t *gr4_args(void) { return json_object(); }
static void gr4_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gr4_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void gr4_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void gr4_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const char *gr4_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
static json_t *gr4_attrerr(const char *tn, const char *name) {
    char m[256];
    snprintf(m, sizeof(m), "'%s' object has no attribute '%s'", tn, name);
    return gr4_raise("AttributeError", m);
}

/* in.state / in.runner accessors */
static const json_t *gr4_state(const json_t *in) { return gr4_oget(in, "state"); }
static const json_t *gr4_sget(const json_t *in, const char *k) {
    return gr4_oget(gr4_state(in), k);
}
static const json_t *gr4_runner(const json_t *in) { return gr4_oget(in, "runner"); }

/* Runner instance-method presence gate + seam (methods come from in.runner). */
static json_t *gr4_runner_method(const json_t *in, const char *name,
                                 json_t *args, const gr4_seams_t *s) {
    const json_t *rn = gr4_runner(in);
    bool present = rn && rn->type == JSON_OBJECT && json_obj_get(rn, name);
    if (!present) {
        json_free(args);
        return gr4_attrerr("GatewayRunner", name);
    }
    return gr4_call(s, name, args);
}

/* PoP: gr4_status_action_label @ gateway/run.py:GatewayRunner._status_action_label */
json_t *gr4_status_action_label(const json_t *in, const gr4_seams_t *s) {
    (void)s;
    const json_t *rr = gr4_sget(in, "_restart_requested");
    if (!rr) return gr4_attrerr("GatewayRunner", "_restart_requested");
    return json_string(gr4_truth(rr) ? "restart" : "shutdown");
}

/* PoP: gr4_status_action_gerund @ gateway/run.py:GatewayRunner._status_action_gerund */
json_t *gr4_status_action_gerund(const json_t *in, const gr4_seams_t *s) {
    const json_t *rr = gr4_sget(in, "_restart_requested");
    if (!rr) return gr4_attrerr("GatewayRunner", "_restart_requested");
    json_t *a = gr4_args();
    gr4_s(a, "key", gr4_truth(rr) ? "gateway.busy.action_restarting"
                                  : "gateway.busy.action_shutting_down");
    return gr4_call(s, "t", a);
}

/* PoP: gr4_sessions_map @ gateway/run.py:GatewayRunner._sessions_map */
json_t *gr4_sessions_map(const json_t *in, const gr4_seams_t *s) {
    (void)s;
    const json_t *sessions = gr4_sget(in, "_sessions");
    /* live: `if sessions is None` -> only a null/absent value is replaced. */
    if (!sessions || sessions->type == JSON_NULL) return json_object();
    return json_copy(sessions);
}

/* PoP: gr4_peek_session_state @ gateway/run.py:GatewayRunner._peek_session_state */
json_t *gr4_peek_session_state(const json_t *in, const gr4_seams_t *s) {
    (void)s;
    const json_t *sessions = gr4_sget(in, "_sessions");
    /* live: `return sessions.get(session_key) if sessions else None` */
    if (!gr4_truth(sessions)) return json_null();
    if (sessions->type != JSON_OBJECT)
        return gr4_attrerr(gr4_type_name(sessions), "get");
    const char *key = json_get_str(in, "session_key", "");
    const json_t *v = json_obj_get(sessions, key);
    return v ? json_copy(v) : json_null();
}

/* PoP: gr4_session_state @ gateway/run.py:GatewayRunner._session_state */
json_t *gr4_session_state(const json_t *in, const gr4_seams_t *s) {
    json_t *sessions = gr4_call(s, "_sessions_map", gr4_args());
    if (gr4_raised(sessions)) return sessions;
    if (!sessions || sessions->type != JSON_OBJECT) {
        json_t *e = gr4_attrerr(gr4_type_name(sessions), "get");
        json_free(sessions);
        return e;
    }
    const char *key = json_get_str(in, "session_key", "");
    const json_t *state = json_obj_get(sessions, key);
    if (state && state->type != JSON_NULL) {
        json_t *out = json_copy(state);
        json_free(sessions);
        return out;
    }
    json_t *ns = gr4_call(s, "SessionState", gr4_args());
    json_free(sessions);
    return ns;
}

/* state.turn.agent read on a materialized attribute double (domain: present).
 * Returns NULL with a raise spec written into *err on a missing carrier. */
static const json_t *gr4_agent_of(const json_t *state, json_t **err) {
    *err = NULL;
    if (!state || state->type == JSON_NULL) return NULL; /* caller handles */
    const json_t *turn = gr4_oget(state, "turn");
    if (!turn || turn->type != JSON_OBJECT) {
        *err = gr4_attrerr("types.SimpleNamespace", "turn");
        return NULL;
    }
    const json_t *agent = json_obj_get(turn, "agent");
    if (!agent) {
        *err = gr4_attrerr("types.SimpleNamespace", "agent");
        return NULL;
    }
    return agent;
}

/* PoP: gr4_is_session_running @ gateway/run.py:GatewayRunner._is_session_running */
json_t *gr4_is_session_running(const json_t *in, const gr4_seams_t *s) {
    json_t *a = gr4_args();
    gr4_s(a, "session_key", json_get_str(in, "session_key", ""));
    json_t *state = gr4_call(s, "_peek_session_state", a);
    if (gr4_raised(state)) return state;
    if (!state || state->type == JSON_NULL) { json_free(state); return json_bool(false); }
    json_t *err = NULL;
    const json_t *agent = gr4_agent_of(state, &err);
    json_free(state);
    if (err) return err;
    return json_bool(agent && agent->type != JSON_NULL);
}

/* PoP: gr4_running_agent_items @ gateway/run.py:GatewayRunner._running_agent_items */
json_t *gr4_running_agent_items(const json_t *in, const gr4_seams_t *s) {
    (void)in;
    json_t *map = gr4_call(s, "_sessions_map", gr4_args());
    if (gr4_raised(map)) return map;
    if (!map || map->type != JSON_OBJECT) {
        json_t *e = gr4_attrerr(gr4_type_name(map), "items");
        json_free(map);
        return e;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < map->c.count; i++) {
        const char *key = map->c.keys[i] ? map->c.keys[i] : "";
        const json_t *st = map->c.items[i];
        json_t *err = NULL;
        const json_t *agent = gr4_agent_of(st, &err);
        if (err) { json_free(map); json_free(out); return err; }
        if (agent && agent->type != JSON_NULL) {
            json_t *pair = json_array();
            json_append(pair, json_string(key));
            json_append(pair, json_copy(agent));
            json_append(out, pair);
        }
    }
    json_free(map);
    return out;
}

/* PoP: gr4_running_agent_count @ gateway/run.py:GatewayRunner._running_agent_count */
json_t *gr4_running_agent_count(const json_t *in, const gr4_seams_t *s) {
    (void)s;
    const json_t *ra = gr4_sget(in, "_running_agents");
    if (!ra) return gr4_attrerr("GatewayRunner", "_running_agents");
    if (ra->type == JSON_ARRAY || ra->type == JSON_OBJECT)
        return json_number((double)ra->c.count);
    if (ra->type == JSON_STRING)
        return json_number((double)strlen(ra->str_val ? ra->str_val : ""));
    char m[160];
    snprintf(m, sizeof(m), "object of type '%s' has no len()", gr4_type_name(ra));
    return gr4_raise("TypeError", m);
}

/* PoP: gr4_resolve_session_key_or_none @ gateway/run.py:GatewayRunner._resolve_session_key_or_none */
json_t *gr4_resolve_session_key_or_none(const json_t *in, const gr4_seams_t *s) {
    const json_t *sk = json_obj_get(in, "session_key");
    const json_t *src = json_obj_get(in, "source");
    bool src_none = !src || src->type == JSON_NULL;
    if (gr4_truth(sk) || src_none) return sk ? json_copy(sk) : json_null();
    json_t *a = gr4_args();
    gr4_j(a, "source", src);
    json_t *r = gr4_call(s, "_session_key_for_source", a);
    if (gr4_raised(r)) {
        if (gr4_is_base_exc(gr4_kind(r))) return r;
        json_free(r);
        return json_null();
    }
    return r;
}

/* PoP: gr4_get_cached_session_source @ gateway/run.py:GatewayRunner._get_cached_session_source */
json_t *gr4_get_cached_session_source(const json_t *in, const gr4_seams_t *s) {
    const char *key = json_get_str(in, "session_key", "");
    if (!key || key[0] == '\0') return json_null(); /* `if session_key` falsy */
    const json_t *cached = gr4_sget(in, "_session_sources");
    if (!gr4_truth(cached)) return json_null();
    if (cached->type != JSON_OBJECT) return gr4_attrerr(gr4_type_name(cached), "get");
    const json_t *source = json_obj_get(cached, key);
    if (!source || source->type == JSON_NULL) return json_null();
    json_t *a = gr4_args();
    gr4_s(a, "key", key);
    json_t *r = gr4_call(s, "move_to_end", a);
    if (gr4_raised(r)) {
        /* `with suppress(Exception)`: Exception swallowed, BaseException passes. */
        if (gr4_is_base_exc(gr4_kind(r))) return r;
        json_free(r);
    } else {
        json_free(r);
    }
    return json_copy(source);
}

/* PoP: gr4_stop_pool @ gateway/run.py:GatewayRunner._stop_pool */
json_t *gr4_stop_pool(const json_t *in, const gr4_seams_t *s) {
    (void)s;
    const json_t *executor = json_obj_get(in, "executor");
    if (!executor || executor->type == JSON_NULL) return json_array();
    json_t *a = gr4_args();
    gr4_b(a, "wait", false);
    gr4_b(a, "cancel_futures", true);
    json_t *r = gr4_call(s, "shutdown", a);
    if (gr4_raised(r)) {
        bool is_te = strcmp(gr4_kind(r), "TypeError") == 0;
        if (!is_te) return r; /* only TypeError is caught by the live except */
        json_free(r);
        json_t *a2 = gr4_args();
        gr4_b(a2, "wait", false);
        json_t *r2 = gr4_call(s, "shutdown", a2);
        if (gr4_raised(r2)) return r2;
        json_free(r2);
    }
    const json_t *threads = gr4_oget(executor, "_threads");
    if (!gr4_truth(threads)) return json_array();
    if (threads->type != JSON_ARRAY) return json_array();
    return json_copy(threads);
}

/* PoP: gr4_get_or_create_pool @ gateway/run.py:GatewayRunner._get_or_create_pool */
json_t *gr4_get_or_create_pool(const json_t *in, const gr4_seams_t *s) {
    const char *attr = json_get_str(in, "attr", "_executor");
    const json_t *lock = gr4_sget(in, "_executor_lock");
    json_t *owned_lock = NULL;
    if (!lock || lock->type == JSON_NULL) {
        owned_lock = gr4_call(s, "Lock", gr4_args());
        if (gr4_raised(owned_lock)) return owned_lock;
        lock = owned_lock;
    }
    json_t *er = gr4_call(s, "lock.enter", gr4_args());
    if (gr4_raised(er)) { json_free(owned_lock); return er; }
    json_free(er);
    json_t *ret = NULL;
    const json_t *closing = gr4_sget(in, "_executor_closing");
    if (gr4_truth(closing)) {
        ret = gr4_raise("RuntimeError", "Gateway is shutting down; executor unavailable");
    } else {
        const json_t *exec = gr4_sget(in, attr);
        bool shutdown = exec && gr4_truth(gr4_oget(exec, "_shutdown"));
        if (!exec || exec->type == JSON_NULL || shutdown) {
            json_t *ma = gr4_args();
            gr4_s(ma, "attr", attr);
            ret = gr4_call(s, "make_pool", ma);
        } else {
            ret = json_copy(exec);
        }
    }
    json_t *xr = gr4_call(s, "lock.exit", gr4_args());
    json_free(owned_lock);
    if (gr4_raised(xr)) { json_free(ret); return xr; }
    json_free(xr);
    return ret;
}

/* PoP: gr4_update_runtime_status @ gateway/run.py:GatewayRunner._update_runtime_status */
json_t *gr4_update_runtime_status(const json_t *in, const gr4_seams_t *s) {
    const json_t *sc = gr4_sget(in, "_startup_cleanup_only");
    if (!sc) return gr4_attrerr("GatewayRunner", "_startup_cleanup_only");
    if (gr4_truth(sc)) return json_null();
    const char *gs = json_get_str(in, "gateway_state", NULL);
    json_t *aw = NULL;
    if (gs && strcmp(gs, "draining") == 0) {
        aw = gr4_call(s, "_describe_active_work", gr4_args());
        if (gr4_raised(aw)) return aw;
    }
    const json_t *rr = gr4_sget(in, "_restart_requested");
    if (!rr) { json_free(aw); return gr4_attrerr("GatewayRunner", "_restart_requested"); }
    json_t *ac = gr4_call(s, "_active_work_count", gr4_args());
    if (gr4_raised(ac)) { json_free(aw); return ac; }
    json_t *a = gr4_args();
    gr4_s(a, "gateway_state", gs);
    gr4_s(a, "exit_reason", json_get_str(in, "exit_reason", NULL));
    gr4_b(a, "restart_requested", gr4_truth(rr));
    gr4_j(a, "active_agents", ac);
    gr4_j(a, "active_work", aw);
    json_free(ac);
    json_free(aw);
    json_t *r = gr4_call(s, "_write_runtime_status_quiet", a);
    if (gr4_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: gr4_session_db_get @ gateway/run.py:GatewayRunner._session_db */
json_t *gr4_session_db_get(const json_t *in, const gr4_seams_t *s) {
    const json_t *pinned = gr4_sget(in, "_session_db_pinned");
    if (!pinned) return gr4_attrerr("GatewayRunner", "_session_db_pinned");
    if (pinned->type == JSON_OBJECT && gr4_oget(pinned, "__unpinned__"))
        return gr4_call(s, "_open_session_db_for_active_scope", gr4_args());
    return json_copy(pinned);
}
