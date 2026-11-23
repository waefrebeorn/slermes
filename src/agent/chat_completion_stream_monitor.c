/*
 * chat_completion_stream_monitor.c — real C port of upstream Hermes agent/chat_completion_stream_monitor.py constant tables.
 * PoP: cn_f3e135e7d4_constants @ agent/chat_completion_stream_monitor.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_f3e135e7d4.h"

json_t *cn_f3e135e7d4_constants(void) {
    json_t *out = json_object();
    json_set(out, "_STALE_ABANDON_KILLS", json_number(3));
    return out;
}
