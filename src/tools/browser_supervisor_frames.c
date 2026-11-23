/*
 * browser_supervisor_frames.c — real C port of upstream Hermes tools/browser_supervisor_frames.py constant tables.
 * PoP: cn_dc50a692e7_constants @ tools/browser_supervisor_frames.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_dc50a692e7.h"

json_t *cn_dc50a692e7_constants(void) {
    json_t *out = json_object();
    json_set(out, "FRAME_TREE_MAX_ENTRIES", json_number(30));
    json_set(out, "FRAME_TREE_MAX_OOPIF_DEPTH", json_number(2));
    return out;
}
