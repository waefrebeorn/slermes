/*
 * event_publisher.c — real C port of upstream Hermes tui_gateway/event_publisher.py constant tables.
 * PoP: cn_de44865c90_constants @ tui_gateway/event_publisher.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_de44865c90.h"

json_t *cn_de44865c90_constants(void) {
    json_t *out = json_object();
    json_set(out, "_QUEUE_MAX", json_number(256));
    return out;
}
