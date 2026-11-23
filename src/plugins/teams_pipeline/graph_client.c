/*
 * graph_client.c — real C port of upstream Hermes plugins/teams_pipeline/graph_client.py constant tables.
 * PoP: cn_a0b4567d4c_constants @ plugins/teams_pipeline/graph_client.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_a0b4567d4c.h"

json_t *cn_a0b4567d4c_constants(void) {
    json_t *out = json_object();
    json_set(out, "DEFAULT_GRAPH_BASE_URL", json_string("https://graph.microsoft.com/v1.0"));
    return out;
}
