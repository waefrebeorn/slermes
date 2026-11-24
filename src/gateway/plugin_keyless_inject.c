/*
 * plugin_keyless_inject.c — C port of upstream Hermes gateway/plugin_keyless_inject.py.
 *
 * PoP: w17_pluginkeyl_d5f537_local @ gateway/plugin_keyless_inject.py:_local
 * PoP: w17_pluginkeyl_d5f537_launched_by_classic_cli @ gateway/plugin_keyless_inject.py:_launched_by_classic_cli
 * PoP: w17_pluginkeyl_d5f537_resolve @ gateway/plugin_keyless_inject.py:resolve
 *
 * The runner/authority objects are injected by the oracle from fixture data;
 * the three seams (_local, _launched_by_classic_cli, authority_for_home /
 * all_authorities) are reproduced here as pure predicates over that data.
 */
#include "noc_w17_pluginkeyl_d5f537.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

int w17_pluginkeyl_d5f537_local(int has_source, int platform_is_local, int authorized) {
    /* Python: source is not None and source.platform == Platform.LOCAL
     *         and authorize_local_source(...) is True  (identity check). */
    return has_source && platform_is_local && authorized == 1;
}

int w17_pluginkeyl_d5f537_launched_by_classic_cli(int refusal, int has_policy, const char *policy_source) {
    if (refusal) return 0;                       /* AuthorityRefusal -> False */
    return has_policy && policy_source != NULL && strcmp(policy_source, "cli") == 0;
}

/* One fixture session -> the facts _local / _launched need. */
typedef struct {
    const char *route;
    int has_source;
    int platform_is_local;
    int authorized;
    int subscribers;
    int policy_refusal;
    const char *policy_source;
} pk_session_t;

static void pk_load_session(const json_t *node, pk_session_t *out) {
    memset(out, 0, sizeof(*out));
    if (!json_is_object(node)) { out->route = ""; return; }
    const json_t *r = json_obj_get(node, "route");
    out->route = (r && r->type == JSON_STRING) ? r->str_val : "";
    const json_t *s = json_obj_get(node, "source");
    out->has_source = json_is_object(s) ? 1 : 0;
    if (out->has_source) {
        const json_t *p = json_obj_get(s, "platform");
        out->platform_is_local = (p && p->type == JSON_STRING && strcmp(p->str_val, "local") == 0) ? 1 : 0;
        out->authorized = json_get_bool(s, "authorized", false) ? 1 : 0;
        out->policy_refusal = json_get_bool(s, "policy_refusal", false) ? 1 : 0;
        const json_t *ps = json_obj_get(s, "policy_source");
        out->policy_source = (ps && ps->type == JSON_STRING) ? ps->str_val : NULL;
    }
    out->subscribers = json_get_bool(node, "subscribers", false) ? 1 : 0;
}

static int pk_is_local(const pk_session_t *s) {
    return w17_pluginkeyl_d5f537_local(s->has_source, s->platform_is_local, s->authorized);
}

static int pk_launched(const pk_session_t *s) {
    return w17_pluginkeyl_d5f537_launched_by_classic_cli(s->policy_refusal, s->policy_source != NULL, s->policy_source);
}

static json_t *pk_result(const char *route) {
    json_t *o = json_object();
    json_set(o, "authority", json_string("own"));
    json_set(o, "route", json_string(route ? route : ""));
    return o;
}

json_t *w17_pluginkeyl_d5f537_resolve(const json_t *fixture) {
    if (!json_is_object(fixture)) return json_null();

    /* authority = authority_for_home(...) if plugin_home is not None else None */
    const json_t *ph = json_obj_get(fixture, "plugin_home");
    int plugin_home_none = !ph || ph->type == JSON_NULL;
    int served = json_get_bool(fixture, "served", false) ? 1 : 0;
    int authority_is_none = plugin_home_none || !served;
    if (authority_is_none) {
        /* _refuse(..., "unserved", ...) -> logged, nothing injected */
        return json_null();
    }

    const char *csk = json_get_str(fixture, "calling_session_key", "");
    const json_t *own = json_obj_get(fixture, "own_sessions");
    const json_t *other = json_obj_get(fixture, "other_sessions");
    size_t n_own = json_is_array(own) ? json_len(own) : 0;
    size_t n_other = json_is_array(other) ? json_len(other) : 0;

    if (csk && csk[0]) {
        /* live = next((s for s in sessions if s.route == csk and _local(runner,s)), None) */
        for (size_t i = 0; i < n_own; i++) {
            pk_session_t s;
            pk_load_session(json_get(own, i), &s);
            /* Return while s is in scope; pk_result copies the string. */
            if (strcmp(s.route, csk) == 0 && pk_is_local(&s)) return pk_result(s.route);
        }
        /* Another profile's live local session with this route? */
        for (size_t i = 0; i < n_other; i++) {
            pk_session_t s;
            pk_load_session(json_get(other, i), &s);
            if (strcmp(s.route, csk) == 0 && s.has_source && s.platform_is_local) {
                return json_null();   /* keyed path's refusal, names plugin's profile */
            }
        }
        return json_null();           /* main's key-less refusal */
    }

    /* attached = [s for s in sessions if s.subscribers and _local and _launched] */
    const char *only_route = NULL;
    int n_attached = 0;
    for (size_t i = 0; i < n_own; i++) {
        pk_session_t s;
        pk_load_session(json_get(own, i), &s);
        if (s.subscribers && pk_is_local(&s) && pk_launched(&s)) {
            n_attached++;
            only_route = s.route; /* borrowed from fixture tree, alive for whole call */
        }
    }
    if (n_attached == 1) return pk_result(only_route);
    /* 0 -> "none", >1 -> "several": both log and return None */
    return json_null();
}
