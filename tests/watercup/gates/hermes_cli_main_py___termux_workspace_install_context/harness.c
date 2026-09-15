/* gate: hermes_cli_main_py___termux_workspace_install_context — traced copy of main_u_termux_workspace_install_context */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_u_termux_workspace_install_context(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_u_termux_workspace_install_context");
    int result = main_u_termux_workspace_install_context(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_u_termux_workspace_install_context");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_u_termux_workspace_install_context <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
