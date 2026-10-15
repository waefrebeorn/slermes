/* gate: port__cli_py__get_toolset_for_tool — traced copy of cli_get_toolset_for_tool (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * cli_get_toolset_for_tool(const char *tool_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_get_toolset_for_tool <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_get_toolset_for_tool");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = cli_get_toolset_for_tool(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_get_toolset_for_tool");
    }
    return 0;
}
