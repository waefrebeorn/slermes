/* gate: hermes_cli_context_switch_guard_py___estimate_tokens — traced copy of estimate_tokens */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int estimate_tokens(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("estimate_tokens");
    int result = estimate_tokens(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("estimate_tokens");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s estimate_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
