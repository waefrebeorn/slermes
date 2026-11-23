/*
 * search.c — real C port of upstream Hermes persistence/conversation/search.py constant tables.
 * PoP: cn_d5adfd101d_constants @ persistence/conversation/search.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_d5adfd101d.h"

json_t *cn_d5adfd101d_constants(void) {
    json_t *out = json_object();
    json_set(out, "MAX_SEARCH_QUERY_CHARS", json_number(2048));
    json_set(out, "FTS_TOOL_CONTENT_PREFIX_CHARS", json_number(8192));
    json_set(out, "SEARCH_RESULT_FIELDS", ({ json_t *__a = json_array();
        json_append(__a, json_string("id"));
        json_append(__a, json_string("session_id"));
        json_append(__a, json_string("role"));
        json_append(__a, json_string("snippet"));
        json_append(__a, json_string("timestamp"));
        json_append(__a, json_string("tool_name"));
        json_append(__a, json_string("source"));
        json_append(__a, json_string("model"));
        json_append(__a, json_string("session_started"));
        json_append(__a, json_string("context"));
      __a; }));
    return out;
}
