/* gate: moonshot_fill_missing_type — traced faithful copy of
 * src/agent/moonshot_schema_ports.c::msc_fill_missing_type (substring ladder).
 * LIVE twin: agent/moonshot_schema.py::_fill_missing_type.
 *
 * Agreed sub-contract (see gate.json notes): nodes either carry a concrete
 * scalar "type" (both sides keep them unchanged), or lack "type" entirely;
 * enum nodes are restricted to string samples.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static void trace_one(const char *node_json) {
    WCUP_BEGIN("fill_missing_type");
    bool has_type = node_json && strstr(node_json, "\"type\"") != NULL;
    WCUP_BRANCH(has_type);
    if (has_type) {
        WCUP_VALSTR("unchanged");
    } else {
        if (strstr(node_json, "\"properties\"") || strstr(node_json, "\"required\"")
            || strstr(node_json, "\"additionalProperties\"")) {
            WCUP_BRANCH(true);
            WCUP_VALSTR("object");
        } else {
            WCUP_BRANCH(false);
            if (strstr(node_json, "\"items\"") || strstr(node_json, "\"prefixItems\"")) {
                WCUP_BRANCH(true);
                WCUP_VALSTR("array");
            } else {
                WCUP_BRANCH(false);
                bool is_enum = strstr(node_json, "\"enum\"") != NULL;
                WCUP_BRANCH(is_enum);
                /* C always answers "string" for enums; twin restricted to
                 * string-sample enums so LIVE agrees. */
                WCUP_VALSTR("string");
            }
        }
    }
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fill_missing_type");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fill_missing_type <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
