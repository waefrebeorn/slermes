/* gate: tools_mcp_tool_py___preflight_content_type — traced copy of mcp_tool_preflight_content_type */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcp_tool_preflight_content_type(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_tool_preflight_content_type");
    bool result = mcp_tool_preflight_content_type(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_tool_preflight_content_type");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_tool_preflight_content_type <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
