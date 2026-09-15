/* gate: hermes_cli_config_py__is_managed — traced copy of cfg_is_managed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cfg_is_managed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cfg_is_managed");
    int result = cfg_is_managed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cfg_is_managed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_is_managed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
