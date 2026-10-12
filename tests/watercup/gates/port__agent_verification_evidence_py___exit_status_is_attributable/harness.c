/* gate: port__agent_verification_evidence_py___exit_status_is_attributable — traced copy of _exit_status_is_attributable (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _exit_status_is_attributable(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _exit_status_is_attributable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_exit_status_is_attributable");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _exit_status_is_attributable(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_exit_status_is_attributable");
    }
    return 0;
}
