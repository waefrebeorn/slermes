/* gate: hermes_cli_main_py___run_npm_watching_for_engine_failure — traced copy of main_g_run_npm_watching_for_engine_failure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_g_run_npm_watching_for_engine_failure(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_g_run_npm_watching_for_engine_failure");
    int result = main_g_run_npm_watching_for_engine_failure(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_g_run_npm_watching_for_engine_failure");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_run_npm_watching_for_engine_failure <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
