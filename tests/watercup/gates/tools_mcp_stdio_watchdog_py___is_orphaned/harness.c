/* gate: tools_mcp_stdio_watchdog_py___is_orphaned — traced copy of tools_mcp_stdio_watchdog_u_is_orphaned */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_mcp_stdio_watchdog_u_is_orphaned(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_mcp_stdio_watchdog_u_is_orphaned");
    int result = tools_mcp_stdio_watchdog_u_is_orphaned(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_mcp_stdio_watchdog_u_is_orphaned");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_mcp_stdio_watchdog_u_is_orphaned <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
