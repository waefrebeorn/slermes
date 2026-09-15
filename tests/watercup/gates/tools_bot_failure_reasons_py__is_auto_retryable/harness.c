/* gate: tools_bot_failure_reasons_py__is_auto_retryable — traced copy of bot_failure_is_auto_retryable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bot_failure_is_auto_retryable(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bot_failure_is_auto_retryable");
    int result = bot_failure_is_auto_retryable(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bot_failure_is_auto_retryable");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bot_failure_is_auto_retryable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
