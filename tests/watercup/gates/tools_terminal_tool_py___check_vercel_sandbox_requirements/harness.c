/* gate: tools_terminal_tool_py___check_vercel_sandbox_requirements — traced copy of termtool_check_vercel_sandbox_requirements */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int termtool_check_vercel_sandbox_requirements(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("termtool_check_vercel_sandbox_requirements");
    int result = termtool_check_vercel_sandbox_requirements(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("termtool_check_vercel_sandbox_requirements");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s termtool_check_vercel_sandbox_requirements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
