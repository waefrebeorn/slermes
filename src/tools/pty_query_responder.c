/*
 * pty_query_responder.c — real C port of upstream Hermes tools/pty_query_responder.py constant tables.
 * PoP: cn_037e736778_constants @ tools/pty_query_responder.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_037e736778.h"

json_t *cn_037e736778_constants(void) {
    json_t *out = json_object();
    json_set(out, "_ESC", json_number(27));
    json_set(out, "_MAX_MODE_DIGITS", json_number(10));
    return out;
}
