/* gate: hermes_cli_main_py___try_redownload_electron_dist — traced copy of mn_try_redownload_electron_dist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mn_try_redownload_electron_dist(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mn_try_redownload_electron_dist");
    bool result = mn_try_redownload_electron_dist(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mn_try_redownload_electron_dist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mn_try_redownload_electron_dist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
