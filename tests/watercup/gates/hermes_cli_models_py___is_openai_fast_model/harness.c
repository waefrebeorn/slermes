/* gate: hermes_cli_models_py___is_openai_fast_model — traced copy of models_is_openai_fast_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int models_is_openai_fast_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("models_is_openai_fast_model");
    int result = models_is_openai_fast_model(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("models_is_openai_fast_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_is_openai_fast_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
