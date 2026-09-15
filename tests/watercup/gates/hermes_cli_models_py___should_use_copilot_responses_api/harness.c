/* gate: hermes_cli_models_py___should_use_copilot_responses_api — traced copy of should_use_copilot_responses_api */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int should_use_copilot_responses_api(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("should_use_copilot_responses_api");
    int result = should_use_copilot_responses_api(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("should_use_copilot_responses_api");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s should_use_copilot_responses_api <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
