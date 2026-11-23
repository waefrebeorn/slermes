/*
 * buzz_cursors.c — real C port of upstream Hermes plugins/platforms/buzz/buzz_cursors.py constant tables.
 * PoP: cn_51f149a2c6_constants @ plugins/platforms/buzz/buzz_cursors.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_51f149a2c6.h"

json_t *cn_51f149a2c6_constants(void) {
    json_t *out = json_object();
    json_set(out, "_SEEN_CAP", json_number(500));
    json_set(out, "_CURSOR_STATE_SUBDIR", json_string("buzz"));
    json_set(out, "_CURSOR_STATE_FILENAME", json_string("channel-cursors.json"));
    return out;
}
