/* gate: schema_sanitizer_strip_slash_enum — traced wrapper around the REAL
 * src/tools/schema_sanitizer.c port (linked via extra_src).
 * LIVE twin: tools/schema_sanitizer.py::strip_slash_enum.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   per tool (loop tick):
 *     B1: tool is a JSON object
 *     B2: function.parameters is a dict
 *     B3: top-level parameters is a dict
 *   V : stripped count (from the REAL C port)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include <json.h>

char *cli_tools_schema_sanitizer__strip_slash_enum(const char *tools_json,
                                                   int *out_stripped);

static void trace_one(const char *tools_json) {
    WCUP_BEGIN("strip_slash_enum");
    json_t *root = json_parse(tools_json, NULL);
    if (!root || root->type != JSON_ARRAY) {
        WCUP_BRANCH(0);
        WCUP_BRANCH(0);
    } else {
        for (size_t i = 0; i < json_len(root); i++) {
            WCUP_LOOP_TICK();
            json_t *tool = json_get(root, i);
            int is_obj = tool && tool->type == JSON_OBJECT;
            WCUP_BRANCH(is_obj);                                  /* B1 */
            if (!is_obj) continue;
            json_t *fn = json_obj_get(tool, "function");
            json_t *params = NULL;
            if (fn && fn->type == JSON_OBJECT)
                params = json_obj_get(fn, "parameters");
            int b2 = fn && fn->type == JSON_OBJECT &&
                     params && params->type == JSON_OBJECT;
            WCUP_BRANCH(b2);                                      /* B2 */
            if (!b2) {
                params = json_obj_get(tool, "parameters");
                WCUP_BRANCH(params && params->type == JSON_OBJECT); /* B3 */
            }
        }
    }
    if (root) json_free(root);

    int stripped = 0;
    char *out = cli_tools_schema_sanitizer__strip_slash_enum(tools_json, &stripped);
    free(out);
    char nbuf[32];
    snprintf(nbuf, sizeof nbuf, "%d", stripped);
    WCUP_VALSTR(nbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("strip_slash_enum");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s strip_slash_enum <tools-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
