/*
 * xai.c — real C port of upstream Hermes hermes_cli/proxy/adapters/xai.py constant tables.
 * PoP: cn_207bbdad81_constants @ hermes_cli/proxy/adapters/xai.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_207bbdad81.h"

json_t *cn_207bbdad81_constants(void) {
    json_t *out = json_object();
    json_set(out, "_POOL_PROVIDER", json_string("xai-oauth"));
    json_set(out, "_ALLOWED_PATHS", ({ json_t *__a = json_array();
        json_append(__a, json_string("/chat/completions"));
        json_append(__a, json_string("/completions"));
        json_append(__a, json_string("/embeddings"));
        json_append(__a, json_string("/models"));
        json_append(__a, json_string("/responses"));
      __a; }));
    return out;
}
