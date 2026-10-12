/* gate: port__tools_skill_manager_tool_py___attach_lint_findings — traced copy of _attach_lint_findings (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _attach_lint_findings(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _attach_lint_findings <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_attach_lint_findings");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _attach_lint_findings(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_attach_lint_findings");
    }
    return 0;
}
