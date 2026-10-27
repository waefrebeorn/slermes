/* gate: port__agent_display_py__build_tool_label — traced copy of TOOL_VERBS (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * TOOL_VERBS(const char *tool);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s TOOL_VERBS <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("TOOL_VERBS");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = TOOL_VERBS(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("TOOL_VERBS");
    }
    return 0;
}
