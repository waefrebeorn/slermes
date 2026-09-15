/* gate: hermes_cli_memory_setup_py___env_line_safe — traced copy of hermes_cli_memory_setup_u_env_line_safe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_memory_setup_u_env_line_safe(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_memory_setup_u_env_line_safe");
    int result = hermes_cli_memory_setup_u_env_line_safe(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_memory_setup_u_env_line_safe");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_memory_setup_u_env_line_safe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
