/* gate: hermes_cli_config_py__ensure_hermes_home — traced copy of cfg_ensure_hermes_home */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cfg_ensure_hermes_home(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cfg_ensure_hermes_home");
    int result = cfg_ensure_hermes_home(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cfg_ensure_hermes_home");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_ensure_hermes_home <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
