/* gate: hermes_cli_config_py__remove_env_value — traced copy of cfg_remove_env_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cfg_remove_env_value(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cfg_remove_env_value");
    int result = cfg_remove_env_value(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cfg_remove_env_value");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_remove_env_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
