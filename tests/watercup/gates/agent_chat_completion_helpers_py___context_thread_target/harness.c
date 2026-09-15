/* gate: agent_chat_completion_helpers_py___context_thread_target — traced copy of cch_context_thread_target */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cch_context_thread_target(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cch_context_thread_target");
    int result = cch_context_thread_target(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cch_context_thread_target");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cch_context_thread_target <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
