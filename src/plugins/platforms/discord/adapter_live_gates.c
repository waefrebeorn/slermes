/*
 * adapter_live_gates.c — real C port of upstream Hermes plugins/platforms/discord/adapter_live_gates.py constant tables.
 * PoP: cn_c9aed165f4_constants @ plugins/platforms/discord/adapter_live_gates.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_c9aed165f4.h"

json_t *cn_c9aed165f4_constants(void) {
    json_t *out = json_object();
    json_set(out, "_LIVE_GATE_EXTRA_KEYS", ({ json_t *__a = json_array();
        json_append(__a, json_string("allow_from"));
        json_append(__a, json_string("allowed_users"));
        json_append(__a, json_string("allowed_roles"));
        json_append(__a, json_string("allowed_channels"));
        json_append(__a, json_string("ignored_channels"));
        json_append(__a, json_string("no_thread_channels"));
        json_append(__a, json_string("free_response_channels"));
        json_append(__a, json_string("allow_all_users"));
        json_append(__a, json_string("allow_bots"));
      __a; }));
    return out;
}
