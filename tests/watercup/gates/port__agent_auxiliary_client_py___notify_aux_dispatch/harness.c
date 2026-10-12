/* gate: port__agent_auxiliary_client_py___notify_aux_dispatch — traced copy of _notify_aux_dispatch (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _notify_aux_dispatch(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _notify_aux_dispatch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_notify_aux_dispatch");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _notify_aux_dispatch(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_notify_aux_dispatch");
    }
    return 0;
}
