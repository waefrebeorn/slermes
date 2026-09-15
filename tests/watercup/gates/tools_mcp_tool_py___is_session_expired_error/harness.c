/* gate: tools_mcp_tool_py___is_session_expired_error — traced copy of mcp_tool_is_session_expired_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcp_tool_is_session_expired_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_tool_is_session_expired_error");
    bool result = mcp_tool_is_session_expired_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_tool_is_session_expired_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_tool_is_session_expired_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
