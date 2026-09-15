/* gate: tools_mcp_tool_py__is_mcp_tool_parallel_safe — traced copy of mcp_is_tool_parallel_safe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcp_is_tool_parallel_safe(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_is_tool_parallel_safe");
    bool result = mcp_is_tool_parallel_safe(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_is_tool_parallel_safe");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_is_tool_parallel_safe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
