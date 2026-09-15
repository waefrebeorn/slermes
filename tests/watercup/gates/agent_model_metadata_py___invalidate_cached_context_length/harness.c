/* gate: agent_model_metadata_py___invalidate_cached_context_length — traced copy of mmd_invalidate_cached_context_length */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mmd_invalidate_cached_context_length(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mmd_invalidate_cached_context_length");
    int result = mmd_invalidate_cached_context_length(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mmd_invalidate_cached_context_length");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mmd_invalidate_cached_context_length <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
