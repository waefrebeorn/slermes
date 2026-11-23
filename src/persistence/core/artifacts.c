/*
 * artifacts.c — real C port of upstream Hermes persistence/core/artifacts.py constant tables.
 * PoP: cn_43db546fa4_constants @ persistence/core/artifacts.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_43db546fa4.h"

json_t *cn_43db546fa4_constants(void) {
    json_t *out = json_object();
    json_set(out, "TRANSCRIPT_SUFFIXES", ({ json_t *__a = json_array();
        json_append(__a, json_string(".json"));
        json_append(__a, json_string(".jsonl"));
      __a; }));
    json_set(out, "REQUEST_DUMP_PATTERN", json_string("request_dump_{session_id}_*.json"));
    return out;
}
