/* gate: tools_delegate_tool_py___is_mcp_toolset_name — traced copy of delegate_is_mcp_toolset_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool delegate_is_mcp_toolset_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("delegate_is_mcp_toolset_name");
    bool result = delegate_is_mcp_toolset_name(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("delegate_is_mcp_toolset_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s delegate_is_mcp_toolset_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
