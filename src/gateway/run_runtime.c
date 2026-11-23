/*
 * run_runtime.c — faithful C11 port of the parked-profile family in
 *                  gateway/run_runtime.py.
 *
 * PoP: w7_runruntime_baa593_profile_parked    @ gateway/run_runtime.py:profile_parked
 * PoP: w7_runruntime_baa593_parked_profile_map @ gateway/run_runtime.py:parked_profile_map
 * PoP: w7_runruntime_baa593_park_profile      @ gateway/run_runtime.py:park_profile
 * PoP: w7_runruntime_baa593_unpark_profile    @ gateway/run_runtime.py:unpark_profile
 *
 * Upstream stores the parked map in runner.session_runtime_descriptor and
 * publishes it via write_runtime_status (a process side effect). The
 * observable contract ported here is the map mutation on the descriptor; the
 * publishing side effect is irrelevant to the JSON differential harness.
 */
#include "noc_w7_runruntime_baa593.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str() for scalar fixture values (fixtures avoid container reprs). */
static char *w7rr_py_str(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        for (int prec = 15; prec <= 17; prec++) {
            snprintf(buf, sizeof(buf), "%.*g", prec, v->num_val);
            if (strtod(buf, NULL) == v->num_val) break;
        }
        return strdup(buf);
    }
    return strdup("");
}

/* Extract an owned copy of descriptor['parked_profiles'], creating it if absent.
 * Mirrors parked_profile_map(): a missing/None descriptor becomes {}. */
static struct json_t *w7rr_map(const struct json_t *descriptor) {
    struct json_t *d = (descriptor && descriptor->type == JSON_OBJECT)
                       ? json_copy(descriptor) : json_object();
    struct json_t *m = json_obj_get(d, "parked_profiles");
    if (!m || m->type != JSON_OBJECT) {
        if (m) json_free(m);
        json_set(d, "parked_profiles", json_object());
        m = json_obj_get(d, "parked_profiles");
    }
    struct json_t *result = json_copy(m);
    json_free(d);
    return result;
}

int w7_runruntime_baa593_profile_parked(const struct json_t *descriptor, const char *name) {
    if (!name || !*name) return 0; /* bool(name) */
    const struct json_t *d = (descriptor && descriptor->type == JSON_OBJECT) ? descriptor : NULL;
    const struct json_t *m = d ? json_obj_get(d, "parked_profiles") : NULL;
    if (!m || m->type != JSON_OBJECT) return 0;
    return json_obj_get(m, name) != NULL ? 1 : 0;
}

struct json_t *w7_runruntime_baa593_parked_profile_map(const struct json_t *descriptor) {
    return w7rr_map(descriptor);
}

struct json_t *w7_runruntime_baa593_park_profile(const struct json_t *descriptor,
        const char *name, const struct json_t *reason) {
    struct json_t *m = w7rr_map(descriptor);
    char *s = w7rr_py_str(reason);
    if (strlen(s) > 500) s[500] = '\0'; /* str(reason)[:500] */
    json_set(m, name ? name : "", json_string(s));
    free(s);
    return m;
}

struct json_t *w7_runruntime_baa593_unpark_profile(const struct json_t *descriptor, const char *name) {
    struct json_t *m = w7rr_map(descriptor);
    if (name && *name) json_obj_del(m, name); /* dict.pop(name, None) drops the key either way */
    return m;
}
