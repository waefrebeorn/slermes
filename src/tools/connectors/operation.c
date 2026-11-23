/*
 * operation.c — real C port of upstream Hermes tools/connectors/operation.py constant tables.
 * PoP: cn_14640a1ed1_constants @ tools/connectors/operation.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_14640a1ed1.h"

json_t *cn_14640a1ed1_constants(void) {
    json_t *out = json_object();
    json_set(out, "OPERATION_DEADLINE_SECONDS", json_number(600));
    return out;
}
