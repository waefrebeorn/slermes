/* gate: hermes_cli_config_py___is_env_config_key — traced copy of config_is_env_config_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool config_is_env_config_key(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("config_is_env_config_key");
    bool result = config_is_env_config_key(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("config_is_env_config_key");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_is_env_config_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
