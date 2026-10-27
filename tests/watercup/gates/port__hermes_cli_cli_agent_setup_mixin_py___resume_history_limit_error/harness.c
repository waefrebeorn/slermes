/* gate: port__hermes_cli_cli_agent_setup_mixin_py___resume_history_limit_error — traced copy of _resume_history_limit_error (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _resume_history_limit_error(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _resume_history_limit_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_resume_history_limit_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _resume_history_limit_error();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_resume_history_limit_error");
    }
    return 0;
}
