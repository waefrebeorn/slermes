/* gate: tools_mcp_tool_py____getattr__ — traced copy of mcp_tool_module_getattr */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcp_tool_module_getattr(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_tool_module_getattr");
    int result = mcp_tool_module_getattr(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_tool_module_getattr");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_tool_module_getattr <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
