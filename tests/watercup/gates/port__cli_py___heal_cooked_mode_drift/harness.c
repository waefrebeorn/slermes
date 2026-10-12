/* gate: port__cli_py___heal_cooked_mode_drift — traced copy of _heal_cooked_mode_drift (int_int) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _heal_cooked_mode_drift(int fd);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _heal_cooked_mode_drift <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_heal_cooked_mode_drift");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _heal_cooked_mode_drift((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_heal_cooked_mode_drift");
    }
    return 0;
}
