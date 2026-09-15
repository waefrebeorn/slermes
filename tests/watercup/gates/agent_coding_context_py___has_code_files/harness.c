/* gate: agent_coding_context_py___has_code_files — traced copy of coding_ctx_has_code_files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool coding_ctx_has_code_files(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("coding_ctx_has_code_files");
    bool result = coding_ctx_has_code_files(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("coding_ctx_has_code_files");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s coding_ctx_has_code_files <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
