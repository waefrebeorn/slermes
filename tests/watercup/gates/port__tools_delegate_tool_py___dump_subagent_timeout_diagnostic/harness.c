/* gate: port__tools_delegate_tool_py___dump_subagent_timeout_diagnostic — traced copy of tools_delegate_tool_u_dump_subagent_timeout_diagnostic (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int tools_delegate_tool_u_dump_subagent_timeout_diagnostic(const char *arg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_delegate_tool_u_dump_subagent_timeout_diagnostic <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_delegate_tool_u_dump_subagent_timeout_diagnostic");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = tools_delegate_tool_u_dump_subagent_timeout_diagnostic(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tools_delegate_tool_u_dump_subagent_timeout_diagnostic");
    }
    return 0;
}
