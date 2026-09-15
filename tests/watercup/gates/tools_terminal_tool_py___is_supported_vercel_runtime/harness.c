/* gate: tools_terminal_tool_py___is_supported_vercel_runtime — traced copy of termtool_is_supported_vercel_runtime */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int termtool_is_supported_vercel_runtime(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("termtool_is_supported_vercel_runtime");
    int result = termtool_is_supported_vercel_runtime(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("termtool_is_supported_vercel_runtime");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s termtool_is_supported_vercel_runtime <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
