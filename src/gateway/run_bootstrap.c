/*
 * run_bootstrap.c — C home for upstream Hermes gateway/run_bootstrap.py.
 *
 * PoP: w8_runbootstra_8ba685_identify_payload @ gateway/run_bootstrap.py:_identify_runtime
 * PoP: w8_runbootstra_8ba685_browse_reconcile_reserve_wait_s @ gateway/run_bootstrap.py:_browse_reconcile_reserve_wait_s
 */
#include "hermes_json.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_runbootstra_8ba685.h"

static bool w8_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }

/* Keys the identify handler copies out of the runtime descriptor, in source order. */
static const char *const W8_DESCRIPTOR_KEYS[] = {
    "instance_id", "runtime_protocol", "authority_epoch", "state", "api_origin",
    "served_profiles", "parked_profiles", "capabilities"
};

json_t *w8_runbootstra_8ba685_identify_payload(const json_t *base,
        const json_t *descriptor, const json_t *draining, const char *restart_wait_reason) {
    json_t *payload = w8_is_obj(base) ? json_copy(base) : json_object();

    if (w8_is_obj(descriptor)) {
        for (size_t i = 0; i < sizeof(W8_DESCRIPTOR_KEYS) / sizeof(W8_DESCRIPTOR_KEYS[0]); i++) {
            json_t *v = json_obj_get(descriptor, W8_DESCRIPTOR_KEYS[i]);
            if (v) json_set(payload, W8_DESCRIPTOR_KEYS[i], json_copy(v));
        }
    }
    json_t *dr = w8_is_obj(descriptor) ? json_obj_get(descriptor, "drain_reason") : NULL;
    bool dr_truthy = dr && dr->type != JSON_NULL
                     && !(dr->type == JSON_STRING && dr->str_val[0] == '\0');

    if (draining && draining->type == JSON_BOOL && draining->bool_val) {
        json_set(payload, "state", json_string("draining"));
        /* descriptor.get('drain_reason') != RESTART_WAIT_REASON */
        bool is_restart_wait = dr && dr->type == JSON_STRING
                               && !strcmp(dr->str_val, restart_wait_reason ? restart_wait_reason : "");
        if (!is_restart_wait) json_set(payload, "capabilities", json_array());
    }
    json_t *state = json_obj_get(payload, "state");
    if (state && state->type == JSON_STRING && !strcmp(state->str_val, "draining") && dr_truthy)
        json_set(payload, "drain_reason", json_copy(dr));

    /* {"manual": "none", "desktop": "none"}.get(supervisor, supervisor or "none")
     * A present null stays null (the default argument is also None); a missing key -> "none". */
    json_t *sup = json_obj_get(payload, "supervisor");
    json_t *mapped;
    if (!sup) mapped = json_string("none");
    else if (sup->type == JSON_NULL) mapped = json_null();
    else if (sup->type == JSON_STRING) {
        const char *sv = sup->str_val;
        mapped = json_string((!strcmp(sv, "manual") || !strcmp(sv, "desktop")) ? "none" : sv);
    } else mapped = json_copy(sup);
    json_set(payload, "supervisor", mapped);
    return payload;
}

json_t *w8_runbootstra_8ba685_browse_reconcile_reserve_wait_s(const json_t *budget) {
    double b = (budget && budget->type == JSON_NUMBER) ? budget->num_val : 0.0;
    return json_number(b + 2.0);
}
