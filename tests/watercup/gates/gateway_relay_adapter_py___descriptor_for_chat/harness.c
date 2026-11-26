/* gate: gateway_relay_adapter_py___descriptor_for_chat — traced copy of descriptor_max_len_for_chat */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int descriptor_max_len_for_chat(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("descriptor_max_len_for_chat");
    int result = descriptor_max_len_for_chat(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("descriptor_max_len_for_chat");
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
