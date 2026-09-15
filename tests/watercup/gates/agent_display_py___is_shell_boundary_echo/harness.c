/* gate: agent_display_py___is_shell_boundary_echo — traced copy of cli_agent_display__is_shell_boundary_echo */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_agent_display__is_shell_boundary_echo(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_agent_display__is_shell_boundary_echo");
    int result = cli_agent_display__is_shell_boundary_echo(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_agent_display__is_shell_boundary_echo");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_display__is_shell_boundary_echo <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
