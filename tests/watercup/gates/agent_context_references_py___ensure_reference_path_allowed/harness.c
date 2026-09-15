/* gate: agent_context_references_py___ensure_reference_path_allowed — traced copy of crf_ensure_reference_path_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool crf_ensure_reference_path_allowed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("crf_ensure_reference_path_allowed");
    bool result = crf_ensure_reference_path_allowed(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("crf_ensure_reference_path_allowed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s crf_ensure_reference_path_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
