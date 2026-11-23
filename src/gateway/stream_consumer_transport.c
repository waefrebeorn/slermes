/*
 * stream_consumer_transport.c — real C port of upstream Hermes gateway/stream_consumer_transport.py constant tables.
 * PoP: cn_1c03c4c0d9_constants @ gateway/stream_consumer_transport.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_1c03c4c0d9.h"

json_t *cn_1c03c4c0d9_constants(void) {
    json_t *out = json_object();
    json_set(out, "_MAX_EDIT_BACKOFF_SECS", json_number(30));
    return out;
}
