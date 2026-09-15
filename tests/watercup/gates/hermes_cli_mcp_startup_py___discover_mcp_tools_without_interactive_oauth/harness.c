/* gate: hermes_cli_mcp_startup_py___discover_mcp_tools_without_interactive_oauth — traced copy of hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th");
    int result = hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
