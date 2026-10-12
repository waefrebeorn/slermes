/* gate: port__cli_py___prepare_deferred_agent_startup — traced copy of _prepare_deferred_agent_startup (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _prepare_deferred_agent_startup(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _prepare_deferred_agent_startup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_prepare_deferred_agent_startup");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _prepare_deferred_agent_startup(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_prepare_deferred_agent_startup");
    }
    return 0;
}
