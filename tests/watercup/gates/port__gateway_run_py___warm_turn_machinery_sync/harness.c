/* gate: port__gateway_run_py___warm_turn_machinery_sync — traced copy of _warm_turn_machinery_sync (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _warm_turn_machinery_sync(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _warm_turn_machinery_sync <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_warm_turn_machinery_sync");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _warm_turn_machinery_sync();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_warm_turn_machinery_sync");
    }
    return 0;
}
