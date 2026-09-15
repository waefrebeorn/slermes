/* gate: hermes_cli_setup_py__run_setup_action_with_navigation — traced copy of run_setup_action_with_navigation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int run_setup_action_with_navigation(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("run_setup_action_with_navigation");
    int result = run_setup_action_with_navigation(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("run_setup_action_with_navigation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s run_setup_action_with_navigation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
