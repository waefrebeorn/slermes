/* gate: hermes_cli_config_py__save_env_value_secure — traced copy of cfg_save_env_value_secure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cfg_save_env_value_secure(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cfg_save_env_value_secure");
    int result = cfg_save_env_value_secure(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cfg_save_env_value_secure");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_save_env_value_secure <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
