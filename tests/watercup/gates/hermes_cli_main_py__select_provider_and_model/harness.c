/* gate: hermes_cli_main_py__select_provider_and_model — traced copy of main_select_provider_and_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_select_provider_and_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_select_provider_and_model");
    int result = main_select_provider_and_model(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_select_provider_and_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_select_provider_and_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
