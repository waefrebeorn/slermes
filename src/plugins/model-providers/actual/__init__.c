/*
 * __init__.c — real C port of upstream Hermes plugins/model-providers/actual/__init__.py constant tables.
 * PoP: cn_fac8f63e4d_constants @ plugins/model-providers/actual/__init__.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_fac8f63e4d.h"

json_t *cn_fac8f63e4d_constants(void) {
    json_t *out = json_object();
    json_set(out, "DEFAULT_ACTUAL_BASE_URL", json_string("https://api.actual.inc/v1"));
    return out;
}
