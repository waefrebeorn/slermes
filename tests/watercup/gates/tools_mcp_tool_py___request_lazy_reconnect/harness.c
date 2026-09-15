/* gate: tools_mcp_tool_py___request_lazy_reconnect — traced copy of mcp_request_lazy_reconnect */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcp_request_lazy_reconnect(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_request_lazy_reconnect");
    bool result = mcp_request_lazy_reconnect(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_request_lazy_reconnect");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_request_lazy_reconnect <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
