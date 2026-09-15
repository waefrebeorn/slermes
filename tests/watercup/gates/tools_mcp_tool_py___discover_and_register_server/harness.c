/* gate: tools_mcp_tool_py___discover_and_register_server — traced copy of mcp_tool_discover_and_register_server */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcp_tool_discover_and_register_server(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_tool_discover_and_register_server");
    int result = mcp_tool_discover_and_register_server(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_tool_discover_and_register_server");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_tool_discover_and_register_server <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
