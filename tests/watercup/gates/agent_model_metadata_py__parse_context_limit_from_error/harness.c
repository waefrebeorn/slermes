/* gate: agent_model_metadata_py__parse_context_limit_from_error — traced copy of parse_context_limit_from_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int parse_context_limit_from_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("parse_context_limit_from_error");
    int result = parse_context_limit_from_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_context_limit_from_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_context_limit_from_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
