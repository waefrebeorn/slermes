/* gate: hermes_state_py___trigram_eligible_tokens — traced copy of hermes_state_trigram_eligible_tokens */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hermes_state_trigram_eligible_tokens(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_state_trigram_eligible_tokens");
    bool result = hermes_state_trigram_eligible_tokens(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_state_trigram_eligible_tokens");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_state_trigram_eligible_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
