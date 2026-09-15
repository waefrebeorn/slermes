/* gate: hermes_cli_main_py___iter_tui_build_inputs — traced copy of main_u_iter_tui_build_inputs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_u_iter_tui_build_inputs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_u_iter_tui_build_inputs");
    int result = main_u_iter_tui_build_inputs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_u_iter_tui_build_inputs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_u_iter_tui_build_inputs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
