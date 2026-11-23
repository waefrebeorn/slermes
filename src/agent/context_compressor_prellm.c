/*
 * context_compressor_prellm.c — real C port of upstream Hermes agent/context_compressor_prellm.py constant tables.
 * PoP: cn_ceab6d16ce_constants @ agent/context_compressor_prellm.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_ceab6d16ce.h"

json_t *cn_ceab6d16ce_constants(void) {
    json_t *out = json_object();
    json_set(out, "_FEASIBILITY_SKIP_MIDDLE_FRACTION", json_number(0.10000000000000001));
    json_set(out, "_FALLBACK_PROBE_AT_MODEL_CONFIG_KEY", json_string("_fallback_probe_at"));
    return out;
}
