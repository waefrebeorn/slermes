/* gate: tools_mcp_dashboard_oauth_py__get_dashboard_oauth_flow — traced copy of tools_mcp_dashboard_oauth_get_dashboard_oauth_flow */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_mcp_dashboard_oauth_get_dashboard_oauth_flow(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_mcp_dashboard_oauth_get_dashboard_oauth_flow");
    int result = tools_mcp_dashboard_oauth_get_dashboard_oauth_flow(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_mcp_dashboard_oauth_get_dashboard_oauth_flow");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_mcp_dashboard_oauth_get_dashboard_oauth_flow <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
