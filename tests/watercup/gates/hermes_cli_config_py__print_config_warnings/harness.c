/* gate: hermes_cli_config_py__print_config_warnings — traced copy of cfg_print_config_warnings */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cfg_print_config_warnings(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cfg_print_config_warnings");
    int result = cfg_print_config_warnings(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cfg_print_config_warnings");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_print_config_warnings <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
