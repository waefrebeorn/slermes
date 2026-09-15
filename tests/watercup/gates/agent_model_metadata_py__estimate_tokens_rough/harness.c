/* gate: agent_model_metadata_py__estimate_tokens_rough — traced copy of estimate_tokens_rough */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int estimate_tokens_rough(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("estimate_tokens_rough");
    int result = estimate_tokens_rough(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("estimate_tokens_rough");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s estimate_tokens_rough <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
