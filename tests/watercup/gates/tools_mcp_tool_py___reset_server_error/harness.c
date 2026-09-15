/* gate: tools_mcp_tool_py___reset_server_error — traced copy of mcp_auth_refresh_if_needed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcp_auth_refresh_if_needed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_auth_refresh_if_needed");
    int result = mcp_auth_refresh_if_needed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_auth_refresh_if_needed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_auth_refresh_if_needed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
