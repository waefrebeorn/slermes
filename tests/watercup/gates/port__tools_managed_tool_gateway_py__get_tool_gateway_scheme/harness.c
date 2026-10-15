/* gate: port__tools_managed_tool_gateway_py__get_tool_gateway_scheme — traced copy of cli_tools_managed_tool_gateway_get_tool_gateway_scheme (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char* cli_tools_managed_tool_gateway_get_tool_gateway_scheme(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_managed_tool_gateway_get_tool_gateway_scheme <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_managed_tool_gateway_get_tool_gateway_scheme");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char* result = cli_tools_managed_tool_gateway_get_tool_gateway_scheme();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_managed_tool_gateway_get_tool_gateway_scheme");
    }
    return 0;
}
