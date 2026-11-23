/*
 * delegation_provider.c — real C port of upstream Hermes agent/delegation_provider.py constant tables.
 * PoP: cn_19fc4acbec_constants @ agent/delegation_provider.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_19fc4acbec.h"

json_t *cn_19fc4acbec_constants(void) {
    json_t *out = json_object();
    json_set(out, "RESERVED_TASK_PARAMETERS", ({ json_t *__a = json_array();
        json_append(__a, json_string("acp_args"));
        json_append(__a, json_string("acp_command"));
        json_append(__a, json_string("api_key"));
        json_append(__a, json_string("api_mode"));
        json_append(__a, json_string("base_url"));
        json_append(__a, json_string("context"));
        json_append(__a, json_string("goal"));
        json_append(__a, json_string("group"));
        json_append(__a, json_string("images"));
        json_append(__a, json_string("index"));
        json_append(__a, json_string("model"));
        json_append(__a, json_string("output_schema"));
        json_append(__a, json_string("provider"));
        json_append(__a, json_string("role"));
        json_append(__a, json_string("toolsets"));
      __a; }));
    json_set(out, "ROUTE_KEYS", ({ json_t *__a = json_array();
        json_append(__a, json_string("api_key"));
        json_append(__a, json_string("api_mode"));
        json_append(__a, json_string("base_url"));
        json_append(__a, json_string("fallback_providers"));
        json_append(__a, json_string("label"));
        json_append(__a, json_string("model"));
        json_append(__a, json_string("profile"));
        json_append(__a, json_string("provider"));
        json_append(__a, json_string("reasoning_effort"));
        json_append(__a, json_string("request_overrides"));
        json_append(__a, json_string("service_tier"));
      __a; }));
    return out;
}
