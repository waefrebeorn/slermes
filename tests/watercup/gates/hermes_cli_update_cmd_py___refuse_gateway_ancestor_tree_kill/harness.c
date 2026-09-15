/* gate: hermes_cli_update_cmd_py___refuse_gateway_ancestor_tree_kill — traced copy of _refuse_gateway_ancestor_tree_kill */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _refuse_gateway_ancestor_tree_kill(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_refuse_gateway_ancestor_tree_kill");
    int result = _refuse_gateway_ancestor_tree_kill(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_refuse_gateway_ancestor_tree_kill");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _refuse_gateway_ancestor_tree_kill <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
