/*
 * run_profile_reconcile.c — C port of upstream Hermes gateway/run_profile_reconcile.py.
 *
 * PoP: w17_runprofile_3fb921_served_profile_names @ gateway/run_profile_reconcile.py:GatewayProfileReconcileMixin.served_profile_names
 * PoP: w17_runprofile_3fb921_added_profile_blocker @ gateway/run_profile_reconcile.py:_added_profile_blocker
 * PoP: w17_runprofile_3fb921_identity_verb_error @ gateway/run_profile_reconcile.py:_identity_verb_error
 *
 * Filesystem/DB-coupled reconcile cycles are runtime-only; these are the pure
 * decision helpers exercised by the differential oracle.
 */
#include "noc_w17_runprofile_3fb921.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RP_OPT_OUT_REASON "gateway.auto_multiplex_migration is false, so this gateway serves only its own profile; run `hermes gateway migrate --multiplex` to serve every profile"

struct json_t *w17_runprofile_3fb921_served_profile_names(const struct json_t *fx) {
    const json_t *homes = json_obj_get(fx, "homes");
    const char *active = json_get_str(fx, "active", "default");
    if (!active || !*active) active = "default"; /* getattr(...) or "default" */
    size_t n = json_is_array(homes) ? json_len(homes) : 0;

    /* active_first = active in homes or not homes */
    int active_present = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(homes, i);
        if (v && v->type == JSON_STRING && strcmp(v->str_val, active) == 0) { active_present = 1; break; }
    }
    int active_first = active_present || n == 0;

    /* Sort the others lexicographically (Python sorted() on str). */
    const char **others = (const char **)calloc(n ? n : 1, sizeof(char *));
    size_t m = 0;
    if (others) {
        for (size_t i = 0; i < n; i++) {
            const json_t *v = json_get(homes, i);
            if (!v || v->type != JSON_STRING) continue;
            if (strcmp(v->str_val, active) == 0) continue;
            others[m++] = v->str_val;
        }
        for (size_t i = 0; i + 1 < m; i++) {
            for (size_t j = i + 1; j < m; j++) {
                if (strcmp(others[i], others[j]) > 0) {
                    const char *t = others[i]; others[i] = others[j]; others[j] = t;
                }
            }
        }
    }

    json_t *out = json_array();
    if (active_first) json_append(out, json_string(active));
    for (size_t i = 0; i < m; i++) json_append(out, json_string(others[i]));
    free(others);
    return out;
}

struct json_t *w17_runprofile_3fb921_added_profile_blocker(const struct json_t *fx) {
    if (json_get_bool(fx, "opted_out", false))
        return json_string(RP_OPT_OUT_REASON);
    const json_t *implicit = json_obj_get(fx, "implicit");
    if (!implicit || implicit->type == JSON_NULL) return json_null();
    return json_copy(implicit);
}

struct json_t *w17_runprofile_3fb921_identity_verb_error(const struct json_t *fx) {
    const char *name = json_get_str(fx, "type_name", "Exception");
    int collision = json_get_bool(fx, "collision", false) ? 1 : 0;
    const char *msg = json_get_str(fx, "message", "");
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s: %s", collision ? "ValueError" : name, msg ? msg : "");
    return json_string(buf);
}
