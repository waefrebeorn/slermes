/*
 * session_hosted_rpc.c — real C port of upstream Hermes gateway/session_hosted_rpc.py constant tables.
 * PoP: cn_4fad7bc068_constants @ gateway/session_hosted_rpc.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_4fad7bc068.h"

json_t *cn_4fad7bc068_constants(void) {
    json_t *out = json_object();
    json_set(out, "_RESULTLESS_OUTCOMES", ({ json_t *__a = json_array();
        json_append(__a, json_string("cancelled"));
        json_append(__a, json_string("interrupted"));
      __a; }));
    return out;
}
