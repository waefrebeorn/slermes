/*
 * update_handoff.c — C home for upstream Hermes hermes_cli/update_handoff.py.
 *
 * PoP: w12_updatehand_b18a8a_is_post_swap_child @ hermes_cli/update_handoff.py:is_post_swap_child
 * PoP: w12_updatehand_b18a8a_takeover_request @ hermes_cli/update_handoff.py:_takeover_request
 *
 * The Windows-CRT setenv("") pitfall is avoided with an explicit env-value seam.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

#include "noc_w12_updatehand_b18a8a.h"

/* Python truthiness for a decoded JSON value. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] != '\0';
    if (v->type == JSON_ARRAY) return json_len(v) > 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) > 0;
    return 0;
}

json_t *w12_updatehand_b18a8a_is_post_swap_child_ex(const char *env_value) {
    return json_bool(env_value && !strcmp(env_value, "1") ? true : false);
}

json_t *w12_updatehand_b18a8a_takeover_request(const json_t *payload, const json_t *argv_tail) {
    json_t *out = json_object();
    if (payload && payload->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(payload); i++) {
            const char *k = json_object_get_key_at(payload, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(payload, i)));
        }
    }
    if (!json_obj_get(out, "desktop")) {
        json_t *had = json_obj_get(payload, "had_desktop_app_before_update");
        json_set(out, "desktop", json_bool(py_truthy(had) ? true : false));
    }
    if (!json_obj_get(out, "windows_resume")) {
        json_t *w = json_obj_get(payload, "windows_gateway_resume");
        json_set(out, "windows_resume", w ? json_copy(w) : json_null());
    }
    if (!json_obj_get(out, "assume_yes")) {
        int yes = 0;
        if (argv_tail && argv_tail->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(argv_tail); i++) {
                json_t *v = json_get(argv_tail, i);
                if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, "--yes")) { yes = 1; break; }
            }
        }
        json_set(out, "assume_yes", json_bool(yes ? true : false));
    }
    if (!json_obj_get(out, "restart_update"))
        json_set(out, "restart_update", json_bool(false));
    return out;
}

void hermes_cli_update_handoff_port_pending(void) { (void)0; }

