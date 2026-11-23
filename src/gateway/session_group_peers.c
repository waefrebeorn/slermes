/*
 * session_group_peers.c - C port of upstream Hermes gateway/session_group_peers.py
 *
 * PoP: w14_sessiongro_4089ba_target_methods @ gateway/session_group_peers.py:TARGET_METHODS
 * PoP: w14_sessiongro_4089ba_room_link @ gateway/session_group_peers.py:room_link
 * PoP: w14_sessiongro_4089ba_invite @ gateway/session_group_peers.py:_invite
 * PoP: w14_sessiongro_4089ba_revoke @ gateway/session_group_peers.py:_revoke
 *
 * Operator-only peer-grant controls. Live gateway/HTTP objects are replaced by
 * JSON seams; the validation and decision logic is ported exactly.
 */
#include "noc_w14_sessiongro_4089ba.h"
#include "hermes_json.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const json_t *og(const json_t *o, const char *k) {
    return json_is_object(o) ? json_obj_get(o, k) : NULL;
}
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static int jbool(const json_t *v, int def) {
    return (v && v->type == JSON_BOOL) ? (v->bool_val ? 1 : 0) : def;
}
static json_t *disabled(const char *reason) {
    json_t *o = json_object();
    json_set(o, "enabled", json_bool(0));
    json_set(o, "reason", json_string(reason));
    return o;
}
static json_t *failure(const char *reason) {
    json_t *o = json_object();
    json_set(o, "error", json_string(reason));
    return o;
}

json_t *w14_sessiongro_4089ba_target_methods(void) {
    json_t *a = json_array();
    json_append(a, json_string("groups.peer.invite"));
    json_append(a, json_string("groups.peer.revoke"));
    return a;
}

json_t *w14_sessiongro_4089ba_room_link(const struct json_t *state) {
    const char *profile = jstr(og(state, "profile"));
    if (!profile) profile = "default";
    if (!jbool(og(state, "adapter"), 0)) return disabled("api_server_required");
    if (!jbool(og(state, "durable"), 0)) return disabled("durable_run_storage_required");
    if (!jbool(og(state, "secret"), 0)) return disabled("gateway_roomlink_secret_unavailable");
    if (jbool(og(state, "catalog_raises"), 0)) return disabled("execution_policy_unsupported");
    const json_t *catalog = og(state, "catalog");
    const json_t *endpoint = og(catalog, "endpoint");
    if (!jbool(og(endpoint, "available"), 0)) return disabled("endpoint_required");
    json_t *o = json_object();
    json_set(o, "enabled", json_bool(1));
    json_set(o, "profile", json_string(profile));
    json_set(o, "catalog", catalog ? json_copy(catalog) : json_object());
    json_set(o, "endpoint", endpoint ? json_copy(endpoint) : json_object());
    return o;
}

json_t *w14_sessiongro_4089ba_invite(const struct json_t *state) {
    const json_t *params = og(state, "params");
    static const char *identity[] = {"room_id", "home_install_id", "authority_gateway_id", "member_id"};
    for (size_t i = 0; i < sizeof identity / sizeof identity[0]; i++) {
        const char *v = jstr(og(params, identity[i]));
        if (!v || !*v) return failure("invalid_params");
    }
    /* authority_epoch arrives as a decimal string to preserve the int64 range. */
    const char *es = jstr(og(params, "authority_epoch"));
    if (!es || !*es) return failure("invalid_params");
    errno = 0;
    char *end = NULL;
    long long epoch = strtoll(es, &end, 10);
    if (errno == ERANGE || !end || *end != '\0' || epoch < 1 || epoch > (long long)INT64_MAX)
        return failure("invalid_params");
    static const char *ttls[] = {"ttl_seconds", "status_ttl_seconds"};
    for (size_t i = 0; i < sizeof ttls / sizeof ttls[0]; i++) {
        const json_t *v = og(params, ttls[i]);
        if (v && v->type != JSON_NUMBER) return failure("invalid_params");
    }
    if (!jbool(og(state, "link_enabled"), 0)) return failure("room_link_unavailable");

    const json_t *inv = og(state, "invitation");
    json_t *o = json_object();
    json_set(o, "grant", inv ? json_copy(og(inv, "grant")) : json_null());
    json_set(o, "target_profile", inv ? json_copy(og(inv, "target_profile")) : json_null());
    const json_t *cat = og(inv, "catalog");
    json_set(o, "catalog", cat ? json_copy(cat) : json_null());
    json_set(o, "endpoint", json_copy(og(cat, "endpoint")));
    return o;
}

json_t *w14_sessiongro_4089ba_revoke(const struct json_t *state) {
    const char *grant = jstr(og(state, "grant"));
    if (!grant || !*grant) return failure("invalid_params");
    if (jbool(og(state, "decode_raises"), 0)) return failure("invalid_room_grant");
    const json_t *claims = og(state, "claims");
    const char *cp = jstr(og(claims, "target_profile"));
    const char *ci = jstr(og(claims, "target_install_id"));
    const char *profile = jstr(og(state, "profile"));
    const char *gid = jstr(og(state, "gateway_id"));
    if (!cp || !ci || !profile || !gid || strcmp(cp, profile) || strcmp(ci, gid))
        return failure("permission_denied");
    json_t *o = json_object();
    json_set(o, "revoked", json_bool(1));
    return o;
}
