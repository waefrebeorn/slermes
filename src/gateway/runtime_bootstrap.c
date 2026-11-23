/*
 * runtime_bootstrap.c — real C port of upstream Hermes gateway/runtime_bootstrap.py constant tables.
 * PoP: cn_325783c15a_constants @ gateway/runtime_bootstrap.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_325783c15a.h"

json_t *cn_325783c15a_constants(void) {
    json_t *out = json_object();
    json_set(out, "_SCOPES", ({ json_t *__a = json_array();
        json_append(__a, json_string("host"));
        json_append(__a, json_string("profile"));
      __a; }));
    return out;
}
