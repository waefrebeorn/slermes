/* gate: utils_py__model_forces_max_completion_tokens — traced copy of util_model_forces_max_completion_tokens */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool util_model_forces_max_completion_tokens(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("util_model_forces_max_completion_tokens");
    bool result = util_model_forces_max_completion_tokens(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("util_model_forces_max_completion_tokens");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s util_model_forces_max_completion_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
