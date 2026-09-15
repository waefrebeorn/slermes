/* gate: hermes_cli_config_py___coerce_config_version — traced copy of config_coerce_config_version */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int config_coerce_config_version(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("config_coerce_config_version");
    int result = config_coerce_config_version(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("config_coerce_config_version");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_coerce_config_version <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
