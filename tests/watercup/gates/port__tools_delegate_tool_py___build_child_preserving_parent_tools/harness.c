/* gate: port__tools_delegate_tool_py___build_child_preserving_parent_tools — traced copy of dlg_build_child_preserving_parent_tools (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int dlg_build_child_preserving_parent_tools(const char *task_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dlg_build_child_preserving_parent_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dlg_build_child_preserving_parent_tools");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = dlg_build_child_preserving_parent_tools(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dlg_build_child_preserving_parent_tools");
    }
    return 0;
}
