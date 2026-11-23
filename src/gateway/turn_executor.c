/*
 * turn_executor.c — real C port of upstream Hermes gateway/turn_executor.py constant tables.
 * PoP: cn_5a9ff58aea_constants @ gateway/turn_executor.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_5a9ff58aea.h"

json_t *cn_5a9ff58aea_constants(void) {
    json_t *out = json_object();
    json_set(out, "_IDLE_TIMEOUT_S", json_number(60));
    return out;
}
