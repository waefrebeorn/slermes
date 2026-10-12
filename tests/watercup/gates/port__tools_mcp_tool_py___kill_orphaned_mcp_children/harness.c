/* gate: port__tools_mcp_tool_py___kill_orphaned_mcp_children — traced copy of mct_kill_orphaned_mcp_children (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern long mct_kill_orphaned_mcp_children(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_kill_orphaned_mcp_children <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("mct_kill_orphaned_mcp_children");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = mct_kill_orphaned_mcp_children((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("mct_kill_orphaned_mcp_children");
    }
    return 0;
}
