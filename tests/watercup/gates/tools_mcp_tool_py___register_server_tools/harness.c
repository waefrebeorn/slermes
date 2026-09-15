/* gate: tools_mcp_tool_py___register_server_tools — traced copy of mcp_tool_register_server_tools */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcp_tool_register_server_tools(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_tool_register_server_tools");
    int result = mcp_tool_register_server_tools(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_tool_register_server_tools");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_tool_register_server_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
