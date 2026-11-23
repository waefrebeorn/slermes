/*
 * adapter_thread_titles.c — real C port of upstream Hermes plugins/platforms/discord/adapter_thread_titles.py constant tables.
 * PoP: cn_187d876a40_constants @ plugins/platforms/discord/adapter_thread_titles.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_187d876a40.h"

json_t *cn_187d876a40_constants(void) {
    json_t *out = json_object();
    json_set(out, "_MAX_RECORDS", json_number(2000));
    return out;
}
