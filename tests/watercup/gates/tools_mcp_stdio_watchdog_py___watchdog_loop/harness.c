/* gate: tools_mcp_stdio_watchdog_py___watchdog_loop — traced copy of tools_mcp_stdio_watchdog_u_watchdog_loop */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_mcp_stdio_watchdog_u_watchdog_loop(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_mcp_stdio_watchdog_u_watchdog_loop");
    int result = tools_mcp_stdio_watchdog_u_watchdog_loop(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_mcp_stdio_watchdog_u_watchdog_loop");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_mcp_stdio_watchdog_u_watchdog_loop <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
