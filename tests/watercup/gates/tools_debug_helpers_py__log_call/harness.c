/* gate: tools_debug_helpers_py__log_call — traced copy of tools_debug_helpers_log_call */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_debug_helpers_log_call(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_debug_helpers_log_call");
    int result = tools_debug_helpers_log_call(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_debug_helpers_log_call");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_debug_helpers_log_call <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
