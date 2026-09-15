/* gate: hermes_cli_update_cmd_py___npm_bin_exists — traced copy of uc_npm_bin_exists */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uc_npm_bin_exists(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uc_npm_bin_exists");
    int result = uc_npm_bin_exists(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uc_npm_bin_exists");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_npm_bin_exists <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
