/*
 * session_discovery.c — C port of upstream Hermes gateway/session_discovery.py.
 *
 * PoP: w12_sessiondis_3b3fbe_discover @ gateway/session_discovery.py:discover_commands
 * PoP: w12_sessiondis_3b3fbe_list_check @ gateway/session_discovery.py:list_plugins
 *
 * Pure authorization/parameter validation of the discovery verbs. All I/O
 * (runtime scope, command catalog, plugin manager) is stubbed in the oracle.
 */
#include "noc_w12_sessiondis_3b3fbe.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *outcome(const char *reason) { return json_string(reason ? reason : "ok"); }

/* 'session:read' in capabilities and matching profile ids. */
static int permitted(const json_t *caps, const json_t *actor, const json_t *authority) {
    int has_read = 0;
    if (json_is_array(caps)) {
        for (size_t i = 0; i < json_len(caps); i++) {
            const json_t *c = json_get(caps, i);
            if (c && c->type == JSON_STRING && c->str_val && !strcmp(c->str_val, "session:read")) { has_read = 1; break; }
        }
    }
    const char *a = (actor && actor->type == JSON_STRING) ? actor->str_val : NULL;
    const char *b = (authority && authority->type == JSON_STRING) ? authority->str_val : NULL;
    return has_read && a && b && !strcmp(a, b);
}

static int key_allowed(const char *k, int completion) {
    if (!strcmp(k, "profile") || !strcmp(k, "session_id")) return 1;
    return completion && !strcmp(k, "text");
}

/* True when params is a dict with only allowed keys and all-string values. */
static int params_valid(const json_t *params, int completion) {
    if (!json_is_object(params)) return 0;
    for (size_t i = 0; i < json_object_size(params); i++) {
        const char *k = json_object_get_key_at(params, i);
        const json_t *v = json_object_get_at(params, i);
        if (!k || !key_allowed(k, completion)) return 0;
        if (!v || v->type != JSON_STRING) return 0;
    }
    return 1;
}

json_t *w12_sessiondis_3b3fbe_discover(const struct json_t *capabilities,
                                        const struct json_t *actor_profile,
                                        const struct json_t *authority_profile,
                                        const struct json_t *params,
                                        const struct json_t *completion,
                                        const struct json_t *profile_ok) {
    if (!permitted(capabilities, actor_profile, authority_profile)) return outcome("permission_denied");
    int comp = completion && completion->type == JSON_BOOL && completion->bool_val;
    if (!params_valid(params, comp)) return outcome("invalid_params");
    const json_t *profile = json_obj_get(params, "profile");
    int ok = !profile_ok || profile_ok->type != JSON_BOOL || profile_ok->bool_val;
    if (profile && profile->type == JSON_STRING && profile->str_val && profile->str_val[0] && !ok)
        return outcome("profile_mismatch");
    return outcome("ok");
}

json_t *w12_sessiondis_3b3fbe_list_check(const struct json_t *capabilities,
                                          const struct json_t *actor_profile,
                                          const struct json_t *authority_profile,
                                          const struct json_t *params) {
    if (!permitted(capabilities, actor_profile, authority_profile)) return outcome("permission_denied");
    int nonempty = params && ((json_is_object(params)) ? json_object_size(params) > 0
                              : json_is_array(params) ? json_len(params) > 0
                              : params->type == JSON_STRING ? params->str_val && params->str_val[0]
                              : params->type == JSON_NUMBER ? params->num_val != 0.0
                              : params->type == JSON_BOOL ? params->bool_val ? 1 : 0
                              : params->type == JSON_NULL ? 0 : 1);
    if (nonempty) return outcome("invalid_params");
    return outcome("ok");
}

