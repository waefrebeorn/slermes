/*
 * session_history_mount.c — faithful C11 port of upstream Hermes
 * gateway/session_history_mount.py (gate w17_sessionhis_4326ac).
 *
 * PoP: w17_sessionhis_4326ac_reader @ gateway/session_history_mount.py:_reader
 * PoP: w17_sessionhis_4326ac_mount_history @ gateway/session_history_mount.py:mount_history
 * PoP: w17_sessionhis_4326ac_is_history_mount @ gateway/session_history_mount.py:is_history_mount
 * PoP: w17_sessionhis_4326ac_require_history_leaf @ gateway/session_history_mount.py:require_history_leaf
 * PoP: w17_sessionhis_4326ac_read_only @ gateway/session_history_mount.py:read_only
 * PoP: w17_sessionhis_4326ac_require_history_access @ gateway/session_history_mount.py:require_history_access
 *
 * Collaborators (authority_persistence, owner_rule, binding_names,
 * store_owner_name, Legacy vocabulary) are scripted via `io` / JSON doubles.
 */
#include "noc_w17_sessionhis_4326ac.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SH_ROUTE_PREFIX "history:"

static bool sh_null(json_t *v) { return !v || v->type == JSON_NULL; }
static json_t *sh_copy(json_t *v) { return v ? json_copy(v) : json_null(); }
static const char *sh_str(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }
static void sh_record(json_t *state, const char *key, json_t *ev) {
    json_t *a = json_obj_get(state, key);
    if (!a) { a = json_array(); json_set(state, key, a); }
    json_append(a, ev);
}
static json_t *sh_refusal(const char *reason, const char *field, const char *detail) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string("AuthorityRefusal"));
    json_set(o, "reason", json_string(reason));
    json_set(o, "field", json_string(field ? field : ""));
    json_set(o, "detail", json_string(detail ? detail : ""));
    return o;
}
static bool sh_in_array(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *v = sh_str(arr->c.items[i]);
        if (v && !strcmp(v, s)) return true;
    }
    return false;
}
static bool sh_in_strs(const char *s, const char *const *v, size_t n) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (!strcmp(s, v[i])) return true;
    return false;
}
static bool sh_contains_all(json_t *setarr, json_t *items) {
    if (!items || items->type != JSON_ARRAY) return true;
    for (size_t i = 0; i < items->c.count; i++) {
        const char *v = sh_str(items->c.items[i]);
        if (!sh_in_array(setarr, v)) return false;
    }
    return true;
}

/* PoP: w17_sessionhis_4326ac_reader */
json_t *w17_sessionhis_4326ac_reader(json_t *authority, json_t *actor, const char *sid,
        json_t *io, json_t *state) {
    (void)state;
    json_t *binding = json_obj_get(io, "history_binding");
    if (!sh_null(binding)) {
        json_t *ev = json_object();
        json_set(ev, "binding", sh_copy(binding));
        json_set(ev, "subject", sh_copy(json_obj_get(actor, "subject")));
        json_set(ev, "rule", json_string("owner"));
        (void)ev;  /* binding_names is scripted; no additional observable state */
        return json_bool(json_get_bool(io, "binding_names", false));
    }
    if (sh_in_array(json_obj_get(actor, "capabilities"), "session:operator"))
        return json_bool(true);
    const char *tid = sh_str(json_obj_get(actor, "transport_id"));
    json_t *transports = sh_null(json_obj_get(authority, "_native_legacy_transports"))
        ? NULL : json_obj_get(authority, "_native_legacy_transports");
    json_t *cand = transports ? json_obj_get(transports, tid ? tid : "") : NULL;
    const char *aid = sh_str(json_obj_get(actor, "id"));
    const char *cv = sh_str(cand);
    return json_bool(cand && !sh_null(cand) && aid && cv && !strcmp(aid, cv));
}

/* PoP: w17_sessionhis_4326ac_is_history_mount */
json_t *w17_sessionhis_4326ac_is_history_mount(json_t *live) {
    if (sh_null(live)) return json_bool(false);
    const char *route = sh_str(json_obj_get(live, "route"));
    json_t *source = json_obj_get(live, "source");
    return json_bool(sh_null(source) && route && strncmp(route, SH_ROUTE_PREFIX, 8) == 0);
}

/* PoP: w17_sessionhis_4326ac_mount_history */
json_t *w17_sessionhis_4326ac_mount_history(json_t *authority, json_t *actor, const char *sid,
        json_t *io, json_t *state) {
    json_t *row = json_obj_get(io, "row");
    const char *source = sh_str(json_obj_get(row, "source"));
    static const char *const LEGACY[4] = {"cli", "tui", "desktop", "webui"};
    /* LEGACY_LOCAL_SOURCES | {'local'} — io.legacy_sources may override the set */
    json_t *legacy = json_obj_get(io, "legacy_sources");
    bool bad_source = legacy
        ? sh_in_array(legacy, source)
        : (sh_in_strs(source, LEGACY, 4) || (source && !strcmp(source, "local")));
    if (bad_source) return json_bool(false);

    const char *profile = sh_str(json_obj_get(row, "profile_name"));
    const char *owner = sh_str(json_obj_get(io, "store_owner_name"));
    const char *expected = (owner && owner[0]) ? owner : "default";
    bool profile_ok = sh_null(json_obj_get(row, "profile_name")) ||
        (profile && profile[0] == '\0') ||
        (profile && expected && !strcmp(profile, expected));
    if (!profile_ok) return json_bool(false);

    if (!json_is_true(w17_sessionhis_4326ac_reader(authority, actor, sid, io, state)))
        return json_bool(false);

    json_t *ab = json_obj_get(io, "api_binding");
    if (json_is_object(ab) && json_obj_get(ab, "$raise")) return json_bool(false);
    if (!sh_null(ab)) return json_bool(false);

    json_t *ev = json_object();
    json_set(ev, "op", json_string("_require_admission_open"));
    sh_record(state, "mount_calls", ev);
    json_t *ev2 = json_object();
    json_set(ev2, "op", json_string("sessions.setdefault"));
    json_set(ev2, "session_id", json_string(sid));
    char route[160];
    snprintf(route, sizeof(route), "%s%s", SH_ROUTE_PREFIX, sid);
    json_set(ev2, "route", json_string(route));
    sh_record(state, "mount_calls", ev2);
    return json_bool(true);
}

/* PoP: w17_sessionhis_4326ac_read_only */
json_t *w17_sessionhis_4326ac_read_only(void) {
    return sh_refusal("session_read_only", "session_id",
        "this stored conversation can be read but not continued; start a new chat");
}

/* PoP: w17_sessionhis_4326ac_require_history_leaf */
json_t *w17_sessionhis_4326ac_require_history_leaf(json_t *authority, const char *sid,
        json_t *io, json_t *state) {
    (void)authority;
    json_t *sessions = json_obj_get(json_obj_get(io, "sessions"), sid);
    if (!json_is_true(w17_sessionhis_4326ac_is_history_mount(sessions))) return json_null();
    json_t *chain = json_obj_get(io, "chain");
    json_t *fp = json_obj_get(io, "footprint");
    json_t *ev = json_object();
    json_set(ev, "op", json_string("lineage.lineage"));
    json_set(ev, "session_id", json_string(sid));
    sh_record(state, "leaf_calls", ev);
    json_t *ev2 = json_object();
    json_set(ev2, "op", json_string("runtime.delete_footprint"));
    json_set(ev2, "session_id", json_string(sid));
    json_set(ev2, "owner", json_string("owner"));
    sh_record(state, "leaf_calls", ev2);
    if (!sh_contains_all(chain, json_obj_get(fp, "targets")) ||
        !sh_contains_all(chain, json_obj_get(fp, "children")))
        return sh_refusal("session_has_children", "session_id",
            "this stored conversation has child sessions a delete would take with it");
    return json_null();
}

/* PoP: w17_sessionhis_4326ac_require_history_access */
json_t *w17_sessionhis_4326ac_require_history_access(json_t *authority, json_t *actor,
        const char *sid, const char *capability, json_t *io, json_t *state) {
    if (!json_is_true(w17_sessionhis_4326ac_reader(authority, actor, sid, io, state)))
        return sh_refusal("not_found", "", "");
    if (!capability || strcmp(capability, "session:read") != 0)
        return w17_sessionhis_4326ac_read_only();
    return json_null();
}
