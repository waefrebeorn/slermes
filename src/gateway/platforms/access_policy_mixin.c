/*
 * access_policy_mixin.c — real C port of upstream Hermes gateway/platforms/access_policy_mixin.py constant tables.
 * PoP: cn_00cfb3ece3_constants @ gateway/platforms/access_policy_mixin.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_00cfb3ece3.h"

json_t *cn_00cfb3ece3_constants(void) {
    json_t *out = json_object();
    json_set(out, "OPTIN_TRUTHY", ({ json_t *__a = json_array();
        json_append(__a, json_string("1"));
        json_append(__a, json_string("true"));
        json_append(__a, json_string("yes"));
      __a; }));
    return out;
}
