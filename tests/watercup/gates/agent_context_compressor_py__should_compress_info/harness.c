/* gate: agent_context_compressor_py__should_compress_info — traced copy of ctxc_should_compress_info */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ctxc_should_compress_info(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ctxc_should_compress_info");
    int result = ctxc_should_compress_info(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ctxc_should_compress_info");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ctxc_should_compress_info <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
