/*
 * api_server_store.c — C home for upstream Hermes gateway/platforms/api_server_store.py.
 * STATUS: BEHAVIOR PORT — value->value store-selection parity, gate w25_apiservers_bf168e.
 * PoP: gateway_platforms_api_server_store_pending @ gateway/platforms/api_server_store.py:<module>
 * PoP: w25_apiservers_bf168e_adapter_persistence @ gateway/platforms/api_server_store.py:adapter_persistence
 * PoP: w25_apiservers_bf168e_selected_session_db @ gateway/platforms/api_server_store.py:selected_session_db
 * PoP: w25_apiservers_bf168e_authority_store @ gateway/platforms/api_server_store.py:authority_store
 * PoP: w25_apiservers_bf168e_delete_removed @ gateway/platforms/api_server_store.py:delete_session
 *
 * The stores are the seam: one SQLite store per profile home, the reserving
 * authority's persistence handle, the adapter's ``_session_db`` override, and
 * the cache lock/closing flag that guards all of them.  This file owns the
 * decision those values make and nothing else -- no connection is opened, no
 * row is deleted, no hook runs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_apiservers_bf168e.h"

/* Python truthiness of a JSON value (absent and null are both falsy). */
static int as_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

json_t *w25_apiservers_bf168e_adapter_persistence(const json_t *request) {
    int runtime = json_get_bool(request, "runtime", false);
    int cache_closed = json_get_bool(request, "cache_closed", false);
    int home_store = json_get_bool(request, "home_store", false);
    int authority_present = json_get_bool(request, "authority_present", false);
    int authority_persistence = json_get_bool(request, "authority_persistence", false);
    const char *override = json_get_str(request, "override", NULL);
    const char *label;

    if (runtime) {
        /* Under a runtime only the reserving authority's handle is answered. */
        if (cache_closed) label = "none";
        else if (authority_present && authority_persistence) label = "authority_store";
        else label = "none";
    } else if (!override) {
        /* No override: the adapter's per-home cache, unless it is closed. */
        label = (!cache_closed && home_store) ? "cache_store" : "none";
    } else {
        /* An override answers only when it is a Storage handle and the cache is open. */
        label = (!cache_closed && !strcmp(override, "handle")) ? "override_store" : "none";
    }
    return json_string(label);
}

json_t *w25_apiservers_bf168e_selected_session_db(const json_t *request) {
    int cache_closed = json_get_bool(request, "cache_closed", false);
    int authority_present = json_get_bool(request, "authority_present", false);
    /* No authority means this runtime never reserved the home; a multiplex
     * route alone is not proof. */
    if (!authority_present) return json_string("none");
    if (cache_closed) return json_string("none");
    return json_string("authority_store");
}

json_t *w25_apiservers_bf168e_authority_store(const json_t *request) {
    json_t *out = json_object();
    if (json_get_bool(request, "has_store", false)) {
        json_set(out, "ok", json_bool(true));
    } else {
        json_set(out, "ok", json_bool(false));
        json_set(out, "kind", json_string("StoreUnavailable"));
        json_set(out, "error", json_string("the authority has no storage interface handle"));
    }
    return out;
}

json_t *w25_apiservers_bf168e_delete_removed(const json_t *request) {
    return json_bool(as_truthy(json_obj_get(request, "removed")) ? true : false);
}
