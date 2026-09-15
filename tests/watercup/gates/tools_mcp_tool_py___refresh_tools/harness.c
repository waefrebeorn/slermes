/* gate: tools_mcp_tool_py___refresh_tools — traced copy of mcp_tool_refresh_tools */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcp_tool_refresh_tools(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_tool_refresh_tools");
    bool result = mcp_tool_refresh_tools(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_tool_refresh_tools");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_tool_refresh_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
