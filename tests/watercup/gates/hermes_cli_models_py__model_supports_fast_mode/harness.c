/* gate: hermes_cli_models_py__model_supports_fast_mode — traced copy of model_supports_fast_mode */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int model_supports_fast_mode(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("model_supports_fast_mode");
    int result = model_supports_fast_mode(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("model_supports_fast_mode");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_supports_fast_mode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
