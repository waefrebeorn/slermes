/*
 * cli_backtrack.c — real C port of upstream Hermes hermes_cli/cli_backtrack.py constant tables.
 * PoP: cn_70050f77ca_constants @ hermes_cli/cli_backtrack.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_70050f77ca.h"

json_t *cn_70050f77ca_constants(void) {
    json_t *out = json_object();
    json_set(out, "_BROWSE_KEYS", ({ json_t *__a = json_array();
        json_append(__a, json_string("left"));
        json_append(__a, json_string("right"));
        json_append(__a, json_string("h"));
        json_append(__a, json_string("l"));
        json_append(__a, json_string("up"));
        json_append(__a, json_string("down"));
        json_append(__a, json_string("j"));
        json_append(__a, json_string("k"));
        json_append(__a, json_string("pageup"));
        json_append(__a, json_string("pagedown"));
        json_append(__a, json_string("enter"));
        json_append(__a, json_string("q"));
        json_append(__a, json_string("f"));
      __a; }));
    return out;
}
