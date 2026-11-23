/*
 * session_reaper.c — C port of upstream Hermes tui_gateway/session_reaper.py.
 *
 * PoP: w14_sessionrea_e9c843_session_has_pending_flush @ tui_gateway/session_reaper.py:_session_has_pending_flush
 * PoP: w14_sessionrea_e9c843_session_is_lru_evictable @ tui_gateway/session_reaper.py:_session_is_lru_evictable
 * PoP: w14_sessionrea_e9c843_session_is_reapable @ tui_gateway/session_reaper.py:_session_is_reapable
 * PoP: w14_sessionrea_e9c843_session_is_evictable @ tui_gateway/session_reaper.py:_session_is_evictable
 *
 * The server-global predicates (_session_pending_kind, _session_has_active_delegations,
 * _session_owns_live_wakeup_schedule, _session_owns_pending_process_notification),
 * _SESSION_TTL_S and the transport sentinel are explicit fixture seams.
 */
#include "noc_w14_sessionrea_e9c843.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}
static int flag(const struct json_t *flags, const char *k) {
    if (!flags || flags->type != JSON_OBJECT) return 0;
    return truthy(json_obj_get(flags, k));
}
static int py_float(const struct json_t *v, double *out) {
    if (!v) { *out = 0.0; return 1; }
    if (v->type == JSON_NULL) { *out = 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL; double d = strtod(v->str_val, &end);
        if (end && *end == '\0') { *out = d; return 1; }
    }
    return 0;
}

/* Python: agent = session.get("agent") if isinstance(session, dict) else None
 * return bool(hasattr(agent, "_persist_session") and getattr(agent, "_session_messages", None)) */
struct json_t *w14_sessionrea_e9c843_session_has_pending_flush(const struct json_t *session) {
    int val = 0;
    if (session && session->type == JSON_OBJECT) {
        const struct json_t *agent = json_obj_get(session, "agent");
        if (agent && agent->type == JSON_OBJECT) {
            const struct json_t *p = json_obj_get(agent, "_persist_session");
            const struct json_t *m = json_obj_get(agent, "_session_messages");
            val = (p != NULL) && truthy(m);   /* hasattr is true even when the value is null */
        }
    }
    return json_bool(val);
}

/* _transport_is_dead: sentinel, fanout(all peers dead), or _closed is True. */
static int transport_dead(const struct json_t *t) {
    if (!t || t->type != JSON_OBJECT) return 0;
    const struct json_t *k = json_obj_get(t, "kind");
    if (k && k->type == JSON_STRING && k->str_val) {
        if (!strcmp(k->str_val, "detached")) return 1;
        if (!strcmp(k->str_val, "fanout")) {
            const struct json_t *peers = json_obj_get(t, "peers");
            if (!peers || peers->type != JSON_ARRAY || json_len(peers) == 0) return 1;
            for (size_t i = 0; i < json_len(peers); ++i)
                if (!transport_dead(json_get(peers, i))) return 0;
            return 1;
        }
    }
    const struct json_t *closed = json_obj_get(t, "closed");
    return closed && closed->type == JSON_BOOL && closed->bool_val;
}

/* _session_is_lru_evictable */
static int lru_evictable(const struct json_t *session, const struct json_t *flags) {
    if (!session || session->type != JSON_OBJECT) return 0;
    if (truthy(json_obj_get(session, "running"))) return 0;
    if (flag(flags, "pending_kind") || flag(flags, "active_delegations")) return 0;
    const struct json_t *ready = json_obj_get(session, "agent_ready");
    if (ready && ready->type == JSON_OBJECT) {
        const struct json_t *is_set = json_obj_get(ready, "set");
        int setv = truthy(is_set);
        if (!setv && !truthy(json_obj_get(session, "lazy"))) return 0;
    }
    return transport_dead(json_obj_get(session, "transport"));
}

struct json_t *w14_sessionrea_e9c843_session_is_lru_evictable(const struct json_t *session, const struct json_t *flags) {
    return json_bool(lru_evictable(session, flags));
}

/* _session_is_reapable */
static int reapable(const struct json_t *session, const struct json_t *flags) {
    return lru_evictable(session, flags) && !flag(flags, "live_wakeup")
           && !flag(flags, "pending_process_notification");
}
struct json_t *w14_sessionrea_e9c843_session_is_reapable(const struct json_t *session, const struct json_t *flags) {
    return json_bool(reapable(session, flags));
}

struct json_t *w14_sessionrea_e9c843_session_is_evictable(const struct json_t *session, const struct json_t *now,
                                        const struct json_t *ttl, const struct json_t *flags) {
    double n = 0.0, t = 0.0, la = 0.0, ca = 0.0;
    py_float(now, &n); py_float(ttl, &t);
    if (session && session->type == JSON_OBJECT) {
        py_float(json_obj_get(session, "last_active"), &la);
        py_float(json_obj_get(session, "created_at"), &ca);
    }
    if ((n - la) <= t || (n - ca) <= t) return json_bool(false);
    return json_bool(reapable(session, flags));
}
