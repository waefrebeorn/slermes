/* gate: tools_mcp_tool_py___parse_boolish — traced copy of mcp_tool_parse_boolish */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcp_tool_parse_boolish(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_tool_parse_boolish");
    bool result = mcp_tool_parse_boolish(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_tool_parse_boolish");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_tool_parse_boolish <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
