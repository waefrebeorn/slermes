/*
 * __init__.c — real C port of upstream Hermes plugins/model-providers/nebius-token-factory/__init__.py constant tables.
 * PoP: cn_0cc94385e1_constants @ plugins/model-providers/nebius-token-factory/__init__.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_0cc94385e1.h"

json_t *cn_0cc94385e1_constants(void) {
    json_t *out = json_object();
    json_set(out, "_REASONING_MARKERS", ({ json_t *__a = json_array();
        json_append(__a, json_string("deepseek-r1"));
        json_append(__a, json_string("deepseek-v4"));
        json_append(__a, json_string("deepseek-reasoner"));
        json_append(__a, json_string("gpt-oss"));
        json_append(__a, json_string("glm-5"));
        json_append(__a, json_string("kimi-k2"));
        json_append(__a, json_string("minimax-m2"));
        json_append(__a, json_string("qwen3"));
      __a; }));
    return out;
}
