/* gate: port__hermes_cli_kanban_swarm_py___create_swarm_uncommitted — traced copy of _create_swarm_uncommitted (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _create_swarm_uncommitted(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _create_swarm_uncommitted <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_create_swarm_uncommitted");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _create_swarm_uncommitted(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_create_swarm_uncommitted");
    }
    return 0;
}
