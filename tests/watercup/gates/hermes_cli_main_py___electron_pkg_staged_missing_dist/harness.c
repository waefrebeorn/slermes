/* gate: hermes_cli_main_py___electron_pkg_staged_missing_dist — traced copy of _electron_pkg_staged_missing_dist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _electron_pkg_staged_missing_dist(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_electron_pkg_staged_missing_dist");
    bool result = _electron_pkg_staged_missing_dist(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_electron_pkg_staged_missing_dist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _electron_pkg_staged_missing_dist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
