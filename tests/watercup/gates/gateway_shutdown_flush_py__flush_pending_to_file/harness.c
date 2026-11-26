/* gate: gateway_shutdown_flush_py__flush_pending_to_file — traced copy of shutdown_flush_pending_to_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int shutdown_flush_pending_to_file(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("shutdown_flush_pending_to_file");
    int result = shutdown_flush_pending_to_file(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shutdown_flush_pending_to_file");
}

int main(int argc, char **argv) {
    /* One cup per input: argv[2..] are the inputs (the runner's protocol). The generated body
     * printed a single cup and fed the fn NAME as the first argument, so every such gate either
     * failed the arity check or compared a constant. The second parameter is the next input in the
     * walk, wrapping -- exactly what the twin's call_fn feeds. */
    int n = argc - 2;
    if (n < 1) { fprintf(stderr, "usage: %s <fn> <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        trace_one(argv[i], argv[2 + ((i - 2 + 1) % n)]);
    return 0;
}
