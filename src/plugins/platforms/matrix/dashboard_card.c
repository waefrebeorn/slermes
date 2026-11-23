/*
 * dashboard_card.c — real C port of upstream Hermes plugins/platforms/matrix/dashboard_card.py constant tables.
 * PoP: cn_fabf8dbd87_constants @ plugins/platforms/matrix/dashboard_card.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_fabf8dbd87.h"

json_t *cn_fabf8dbd87_constants(void) {
    json_t *out = json_object();
    json_set(out, "DASHBOARD_CARD", ({ json_t *__o = json_object();
        json_set(__o, "dashboard_description", json_string("Use Hermes in Matrix rooms and direct messages."));
        json_set(__o, "dashboard_env_vars", ({ json_t *__a = json_array();
            json_append(__a, json_string("MATRIX_HOMESERVER"));
            json_append(__a, json_string("MATRIX_ACCESS_TOKEN"));
            json_append(__a, json_string("MATRIX_USER_ID"));
            json_append(__a, json_string("MATRIX_ALLOWED_USERS"));
          __a; }));
        json_set(__o, "dashboard_required_env", ({ json_t *__a = json_array();
            json_append(__a, json_string("MATRIX_HOMESERVER"));
            json_append(__a, json_string("MATRIX_ACCESS_TOKEN"));
            json_append(__a, json_string("MATRIX_USER_ID"));
          __a; }));
        json_set(__o, "docs_url", json_string("https://matrix.org/ecosystem/servers/"));
      __o; }));
    return out;
}
