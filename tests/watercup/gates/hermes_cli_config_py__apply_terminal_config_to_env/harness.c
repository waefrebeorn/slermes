/* gate: hermes_cli_config_py__apply_terminal_config_to_env — traced copy of cfg_apply_terminal_config_to_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cfg_apply_terminal_config_to_env(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cfg_apply_terminal_config_to_env");
    int result = cfg_apply_terminal_config_to_env(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cfg_apply_terminal_config_to_env");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_apply_terminal_config_to_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
