/* gate: agent_agent_init_py___custom_provider_model_matches — traced copy of agi_custom_provider_model_matches */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agi_custom_provider_model_matches(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agi_custom_provider_model_matches");
    int result = agi_custom_provider_model_matches(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agi_custom_provider_model_matches");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agi_custom_provider_model_matches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
