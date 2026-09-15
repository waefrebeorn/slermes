/* gate: tools_thread_context_py__propagate_context_to_thread — traced copy of tools_thread_context_propagate_context_to_thread */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_thread_context_propagate_context_to_thread(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_thread_context_propagate_context_to_thread");
    int result = tools_thread_context_propagate_context_to_thread(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_thread_context_propagate_context_to_thread");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_thread_context_propagate_context_to_thread <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
