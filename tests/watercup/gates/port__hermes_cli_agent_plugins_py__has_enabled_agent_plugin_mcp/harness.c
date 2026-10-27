/* gate: port__hermes_cli_agent_plugins_py__has_enabled_agent_plugin_mcp — traced copy of ap_has_enabled_agent_plugin_mcp (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int ap_has_enabled_agent_plugin_mcp(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ap_has_enabled_agent_plugin_mcp <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ap_has_enabled_agent_plugin_mcp");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = ap_has_enabled_agent_plugin_mcp();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ap_has_enabled_agent_plugin_mcp");
    }
    return 0;
}
