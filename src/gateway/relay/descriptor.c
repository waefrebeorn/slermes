/*
 * descriptor.c — real C port of upstream Hermes gateway/relay/descriptor.py constant tables.
 * PoP: cn_08669ae43f_constants @ gateway/relay/descriptor.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_08669ae43f.h"

json_t *cn_08669ae43f_constants(void) {
    json_t *out = json_object();
    json_set(out, "CONTRACT_VERSION", json_number(1));
    return out;
}
