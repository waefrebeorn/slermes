/* gate: agent_context_compressor_py__rollback_interrupted_preflight_display_tokens — traced copy of ctxc_rollback_interrupted_preflight_display_tokens */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ctxc_rollback_interrupted_preflight_display_tokens(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ctxc_rollback_interrupted_preflight_display_tokens");
    int result = ctxc_rollback_interrupted_preflight_display_tokens(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ctxc_rollback_interrupted_preflight_display_tokens");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ctxc_rollback_interrupted_preflight_display_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
