/* gate: agent_context_compressor_py__has_content_to_compress — traced copy of cc_has_content_to_compress */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cc_has_content_to_compress(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cc_has_content_to_compress");
    bool result = cc_has_content_to_compress(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cc_has_content_to_compress");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cc_has_content_to_compress <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
