/* gate: tools_delegation_live_log_py__thinking — traced copy of dll_thinking */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dll_thinking(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dll_thinking");
    int result = dll_thinking(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dll_thinking");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dll_thinking <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
