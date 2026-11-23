/*
 * session_hosted_controls.c — real C port of upstream Hermes gateway/session_hosted_controls.py constant tables.
 * PoP: cn_821f2df8af_constants @ gateway/session_hosted_controls.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_821f2df8af.h"

json_t *cn_821f2df8af_constants(void) {
    json_t *out = json_object();
    json_set(out, "_GRANT_ERRORS", ({ json_t *__a = json_array();
        json_append(__a, json_string("invalid_room_grant"));
        json_append(__a, json_string("room_reauthorization_required"));
      __a; }));
    return out;
}
