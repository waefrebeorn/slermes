/*
 * session_contract.c — real C port of upstream Hermes gateway/session_contract.py constant tables.
 * PoP: cn_d06d1d1eb5_constants @ gateway/session_contract.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_d06d1d1eb5.h"

json_t *cn_d06d1d1eb5_constants(void) {
    json_t *out = json_object();
    json_set(out, "CANONICAL_GATEWAY_PROTOCOL", json_string("hermes-gateway-v1"));
    return out;
}
