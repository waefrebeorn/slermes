/* gate: tools_mcp_tool_py___run_on_mcp_loop — traced copy of mct_run_on_mcp_loop */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mct_run_on_mcp_loop(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mct_run_on_mcp_loop");
    int result = mct_run_on_mcp_loop(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mct_run_on_mcp_loop");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_run_on_mcp_loop <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
