/* gate: tools_mcp_tool_py__shutdown_mcp_servers — traced copy of mcp_remove_server */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcp_remove_server(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_remove_server");
    bool result = mcp_remove_server(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_remove_server");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_remove_server <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
