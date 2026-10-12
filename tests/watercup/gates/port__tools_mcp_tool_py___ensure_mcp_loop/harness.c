/* gate: port__tools_mcp_tool_py___ensure_mcp_loop — traced copy of mct_ensure_mcp_loop (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int mct_ensure_mcp_loop(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_ensure_mcp_loop <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("mct_ensure_mcp_loop");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = mct_ensure_mcp_loop();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("mct_ensure_mcp_loop");
    }
    return 0;
}
