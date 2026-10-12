/* gate: port__tools_mcp_dashboard_oauth_py__dashboard_oauth_flow — traced copy of tools_mcp_dashboard_oauth_dashboard_oauth_flow (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int tools_mcp_dashboard_oauth_dashboard_oauth_flow(const char *arg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_mcp_dashboard_oauth_dashboard_oauth_flow <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_mcp_dashboard_oauth_dashboard_oauth_flow");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = tools_mcp_dashboard_oauth_dashboard_oauth_flow(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tools_mcp_dashboard_oauth_dashboard_oauth_flow");
    }
    return 0;
}
