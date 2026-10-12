/* gate: port__agent_auxiliary_client_py___aux_thread_local_hook — traced copy of _aux_thread_local_hook (double_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern double _aux_thread_local_hook(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _aux_thread_local_hook <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_aux_thread_local_hook");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = _aux_thread_local_hook(strtod(argv[i], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_aux_thread_local_hook");
    }
    return 0;
}
