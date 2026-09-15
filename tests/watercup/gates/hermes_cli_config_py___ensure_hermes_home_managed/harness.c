/* gate: hermes_cli_config_py___ensure_hermes_home_managed — traced copy of config_py_ensure_hermes_home_managed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int config_py_ensure_hermes_home_managed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("config_py_ensure_hermes_home_managed");
    int result = config_py_ensure_hermes_home_managed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("config_py_ensure_hermes_home_managed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_py_ensure_hermes_home_managed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
