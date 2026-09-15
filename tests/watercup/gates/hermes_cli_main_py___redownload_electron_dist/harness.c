/* gate: hermes_cli_main_py___redownload_electron_dist — traced copy of main_u_redownload_electron_dist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_u_redownload_electron_dist(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_u_redownload_electron_dist");
    int result = main_u_redownload_electron_dist(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_u_redownload_electron_dist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_u_redownload_electron_dist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
