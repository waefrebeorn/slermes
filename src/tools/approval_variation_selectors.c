/*
 * approval_variation_selectors.c — real C port of upstream Hermes tools/approval_variation_selectors.py constant tables.
 * PoP: cn_d6a5548205_constants @ tools/approval_variation_selectors.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_d6a5548205.h"

json_t *cn_d6a5548205_constants(void) {
    json_t *out = json_object();
    json_set(out, "_EMOJI_RANGE", json_string("\\U0001F000-\\U0001FAFF\\u2600-\\u27BF"));
    return out;
}
