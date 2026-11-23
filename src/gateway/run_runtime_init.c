/*
 * run_runtime_init.c — C home for upstream Hermes gateway/run_runtime_init.py.
 *
 * PoP: w10_runruntime_519a08_persistence @ gateway/run_runtime_init.py:GatewayRuntimeInitMixin.persistence
 * PoP: w10_runruntime_519a08_profile_name @ gateway/run_runtime_init.py:GatewayRuntimeInitMixin._init_runtime_caches
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w10_runruntime_519a08.h"

/* Mirrors the persistence property with the sentinel collapsed into "pinned":
 *   pinned -> None
 *   no session_store -> _open_session_db_for_active_scope()  (injected "scope_result")
 *   otherwise -> session_store.persistence                   (injected "store_persistence") */
json_t *w10_runruntime_519a08_persistence(const json_t *in) {
    if (json_get_bool(in, "pinned", 0)) return json_null();
    if (!json_get_bool(in, "has_store", 0)) {
        const json_t *scope = json_obj_get(in, "scope_result");
        return scope ? json_copy(scope) : json_null();
    }
    const json_t *sp = json_obj_get(in, "store_persistence");
    return sp ? json_copy(sp) : json_null();
}

/* Mirrors _init_runtime_caches' identity wiring:
 *   launch = self._active_profile_name()
 *   self._kanban_notifier_profile = launch
 *   self._primary_profile_name = "default" if multiplex_profiles else launch */
json_t *w10_runruntime_519a08_profile_name(const json_t *in) {
    const char *launch = json_get_str(in, "launch", "");
    int multiplexed = json_get_bool(in, "multiplexed", 0);
    json_t *out = json_object();
    json_set(out, "primary", json_string(multiplexed ? "default" : (launch ? launch : "")));
    json_set(out, "kanban", json_string(launch ? launch : ""));
    return out;
}
