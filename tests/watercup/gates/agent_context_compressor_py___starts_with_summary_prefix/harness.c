/* gate: agent_context_compressor_py___starts_with_summary_prefix — traced copy of context_compressor__starts_with_summary_prefix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int context_compressor__starts_with_summary_prefix(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("context_compressor__starts_with_summary_prefix");
    int result = context_compressor__starts_with_summary_prefix(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("context_compressor__starts_with_summary_prefix");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s context_compressor__starts_with_summary_prefix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
