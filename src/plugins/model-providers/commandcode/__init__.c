/*
 * __init__.c — real C port of upstream Hermes plugins/model-providers/commandcode/__init__.py constant tables.
 * PoP: cn_a2ea107778_constants @ plugins/model-providers/commandcode/__init__.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_a2ea107778.h"

json_t *cn_a2ea107778_constants(void) {
    json_t *out = json_object();
    json_set(out, "_COMMANDCODE_BASE", json_string("https://api.commandcode.ai/provider/v1"));
    return out;
}
