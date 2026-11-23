/*
 * session_readiness.c — C home for upstream Hermes gateway/session_readiness.py.
 *
 * PoP: w12_sessionrea_defea4_guard @ gateway/session_readiness.py:readiness
 */
#include "noc_w12_sessionrea_defea4.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static char *reason(const char *s) {
    size_t n = strlen(s) + 1;
    char *out = (char *)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static int caps_has(const json_t *caps, const char *needle) {
    if (!caps || caps->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(caps); ++i) {
        json_t *v = json_get(caps, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, needle)) return 1;
    }
    return 0;
}

char *w12_sessionrea_defea4_guard(const struct json_t *caps,
                                   const char *actor_profile_id,
                                   const char *authority_profile_id,
                                   const struct json_t *params,
                                   int runtime,
                                   const char *resolved_profile_dir,
                                   int resolve_raises) {
    const char *ap = actor_profile_id ? actor_profile_id : "";
    const char *authp = authority_profile_id ? authority_profile_id : "";

    /* if 'session:create' not in actor.capabilities or actor.profile_id != authority.profile_id */
    if (!caps_has(caps, "session:create") || strcmp(ap, authp) != 0)
        return reason("permission_denied");

    /* allowed = {'profile','provider'} if runtime else {'profile'} */
    if (!params || params->type != JSON_OBJECT)
        return reason("invalid_params");
    for (size_t i = 0; i < json_object_size(params); ++i) {
        const char *k = json_object_get_key_at(params, i);
        if (!k) return reason("invalid_params");
        if (strcmp(k, "profile") && !(runtime && !strcmp(k, "provider")))
            return reason("invalid_params");
        json_t *v = json_object_get_at(params, i);
        if (!v || v->type != JSON_STRING) return reason("invalid_params");
    }

    /* if params.get('profile'): resolve and compare paths */
    json_t *prof = json_obj_get(params, "profile");
    if (prof && prof->type == JSON_STRING && prof->str_val && prof->str_val[0] != '\0') {
        if (resolve_raises) return reason("profile_mismatch");
        const char *resolved = resolved_profile_dir ? resolved_profile_dir : "";
        if (strcmp(resolved, authp) != 0) return reason("profile_mismatch");
    }

    return NULL; /* guard allows the probe */
}
