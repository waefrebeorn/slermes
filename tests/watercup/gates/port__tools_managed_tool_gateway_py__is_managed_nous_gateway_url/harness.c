/* gate: port__tools_managed_tool_gateway_py__is_managed_nous_gateway_url — traced copy of cli_tools_managed_tool_gateway_is_managed_nous_gateway_url (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_tools_managed_tool_gateway_is_managed_nous_gateway_url(const char *url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_managed_tool_gateway_is_managed_nous_gateway_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_managed_tool_gateway_is_managed_nous_gateway_url");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_managed_tool_gateway_is_managed_nous_gateway_url(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_managed_tool_gateway_is_managed_nous_gateway_url");
    }
    return 0;
}
