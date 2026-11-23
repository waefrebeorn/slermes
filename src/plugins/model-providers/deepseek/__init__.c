/*
 * __init__.c — real C port of upstream Hermes plugins/model-providers/deepseek/__init__.py constant tables.
 * PoP: cn_0727c5d06d_constants @ plugins/model-providers/deepseek/__init__.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_0727c5d06d.h"

json_t *cn_0727c5d06d_constants(void) {
    json_t *out = json_object();
    json_set(out, "_THINKING_CAPABLE_IDS", ({ json_t *__a = json_array();
        json_append(__a, json_string("deepseek-flash"));
      __a; }));
    return out;
}
