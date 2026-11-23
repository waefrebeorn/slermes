/*
 * _common.c — real C port of upstream Hermes plugins/browser/_common.py constant tables.
 * PoP: cn_42052f4e41_constants @ plugins/browser/_common.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_42052f4e41.h"

json_t *cn_42052f4e41_constants(void) {
    json_t *out = json_object();
    json_set(out, "_CLOSE_OK", ({ json_t *__a = json_array();
        json_append(__a, json_number(200));
        json_append(__a, json_number(201));
        json_append(__a, json_number(204));
      __a; }));
    return out;
}
