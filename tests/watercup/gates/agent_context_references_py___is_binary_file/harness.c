/* gate: agent_context_references_py___is_binary_file — traced copy of crf_is_binary_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool crf_is_binary_file(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("crf_is_binary_file");
    bool result = crf_is_binary_file(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("crf_is_binary_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s crf_is_binary_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
