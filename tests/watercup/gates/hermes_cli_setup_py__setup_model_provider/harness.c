/* gate: hermes_cli_setup_py__setup_model_provider — traced copy of setup_model_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int setup_model_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("setup_model_provider");
    int result = setup_model_provider(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("setup_model_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s setup_model_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
