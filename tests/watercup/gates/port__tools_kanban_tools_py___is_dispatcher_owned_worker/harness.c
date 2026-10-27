/* gate: port__tools_kanban_tools_py___is_dispatcher_owned_worker — traced copy of _is_dispatcher_owned_worker (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _is_dispatcher_owned_worker(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_dispatcher_owned_worker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_dispatcher_owned_worker");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _is_dispatcher_owned_worker();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_dispatcher_owned_worker");
    }
    return 0;
}
