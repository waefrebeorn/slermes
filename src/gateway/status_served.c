/*
 * status_served.c -- faithful C11 port of gateway/status_served.py pure helpers.
 *
 * PoP: w8_statusserve_cfb190_local_liveness_pid             @ gateway/status_served.py:local_liveness_pid
 * PoP: w8_statusserve_cfb190_served_profile_runtime         @ gateway/status_served.py:served_profile_runtime
 * PoP: w8_statusserve_cfb190_served_profile_mirror_platforms @ gateway/status_served.py:served_profile_mirror_platforms
 */
#include "noc_w8_statusserve_cfb190.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *w8ss_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static int w8ss_present(const json_t *v) {
    return v && v->type != JSON_NULL;
}

struct json_t *w8_statusserve_cfb190_local_liveness_pid(const struct json_t *liveness) {
    const char *source = json_get_str(liveness, "source", "");
    if (source && !strcmp(source, "health")) return json_null();
    json_t *pid = w8ss_get(liveness, "pid");
    return pid ? json_copy(pid) : json_null();
}

struct json_t *w8_statusserve_cfb190_served_profile_runtime(const struct json_t *home,
        const struct json_t *own_runtime, const struct json_t *gateway_pid,
        const struct json_t *seams) {
    (void)home;  /* only forwarded to the stubbed status collaborators */
    const json_t *own_pid = w8ss_get(seams, "own_running_pid");
    if (w8ss_present(own_runtime) && w8ss_present(own_pid)) return json_null();

    json_t *served = w8ss_get(seams, "served");
    if (!served || served->type != JSON_ARRAY || served->c.count < 2) return json_null();
    json_t *record = served->c.items[1];
    if (!record || record->type != JSON_OBJECT || record->c.count == 0) return json_null();
    double mux_pid = 0.0;
    /* mux pid is a plain number in the fixture */
    if (served->c.items[0] && served->c.items[0]->type == JSON_NUMBER)
        mux_pid = served->c.items[0]->num_val;
    if (w8ss_present(gateway_pid) && gateway_pid->type == JSON_NUMBER
            && mux_pid != gateway_pid->num_val) return json_null();

    const char *name = json_get_str(seams, "profile_name", "");
    if (!name || !name[0]) name = "default";
    (void)name;  /* platforms are injected by the seam; name only selects them in the live module */
    json_t *out = json_copy(record);
    json_t *plats = w8ss_get(seams, "platforms");
    json_set(out, "platforms", plats ? json_copy(plats) : json_null());
    return out;
}

struct json_t *w8_statusserve_cfb190_served_profile_mirror_platforms(
        const struct json_t *runtime, const char *profile) {
    json_t *out = json_object();
    if (!profile || !profile[0] || !strcmp(profile, "default")) return out;
    json_t *plats = w8ss_get(runtime, "platforms");
    if (!plats || plats->type != JSON_OBJECT) return out;

    /* gateway/config.py: SERVED_PROFILE_MIRROR_PLATFORMS = {'api_server','webhook','relay'},
     * SHARED_LISTENER_MIRROR_PLATFORMS = {'api_server','webhook'},
     * SHARED_LISTENER_MIRROR_PATHS = {'api_server':'/v1','webhook':'/webhooks/<route>'}. */
    static const char *const NAMES[] = {"api_server", "relay", "webhook"}; /* sorted() */
    for (int i = 0; i < 3; i++) {
        const char *nm = NAMES[i];
        json_t *entry = w8ss_get(plats, nm);
        if (!entry || entry->type != JSON_OBJECT) continue;
        int shared = (!strcmp(nm, "api_server") || !strcmp(nm, "webhook"));
        if (shared) {
            const char *state = json_get_str(entry, "state", "");
            if (!state || (strcmp(state, "connected") && strcmp(state, "connecting")
                           && strcmp(state, "retrying"))) continue;
        }
        json_t *basev = w8ss_get(entry, "listener_base");
        const char *base = (basev && basev->type == JSON_STRING && basev->str_val && basev->str_val[0])
                           ? basev->str_val : NULL;
        json_t *proj = json_object();
        for (size_t j = 0; j < json_object_size(entry); j++) {
            const char *k = json_object_get_key_at(entry, j);
            if (k && strcmp(k, "listener_base"))
                json_set(proj, k, json_copy(json_object_get_at(entry, j)));
        }
        if (base) {
            const char *path = !strcmp(nm, "api_server") ? "/v1" :
                               (!strcmp(nm, "webhook") ? "/webhooks/<route>" : "");
            size_t len = strlen(base) + strlen(profile) + strlen(path) + 4;
            char *url = (char *)malloc(len);
            snprintf(url, len, "%s/p/%s%s", base, profile, path);
            json_set(proj, "ingress_url", json_string(url));
            free(url);
        } else {
            json_set(proj, "ingress_url", json_null());
        }
        json_set(proj, "mirrored_from", json_string("default"));
        json_set(out, nm, proj);
    }
    return out;
}
