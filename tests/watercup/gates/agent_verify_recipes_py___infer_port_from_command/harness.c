/* gate: agent_verify_recipes_py___infer_port_from_command — traced copy of recipe_infer_port_from_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int recipe_infer_port_from_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("recipe_infer_port_from_command");
    int result = recipe_infer_port_from_command(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("recipe_infer_port_from_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s recipe_infer_port_from_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
