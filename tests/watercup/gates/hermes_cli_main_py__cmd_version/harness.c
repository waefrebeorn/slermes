/* gate: hermes_cli_main_py__cmd_version — traced copy of main_cmd_version */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_cmd_version(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_cmd_version");
    int result = main_cmd_version(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_cmd_version");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_cmd_version <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
