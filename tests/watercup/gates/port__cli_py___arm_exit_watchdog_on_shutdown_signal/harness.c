/* gate: port__cli_py___arm_exit_watchdog_on_shutdown_signal — traced copy of _arm_exit_watchdog_on_shutdown_signal (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _arm_exit_watchdog_on_shutdown_signal(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _arm_exit_watchdog_on_shutdown_signal <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_arm_exit_watchdog_on_shutdown_signal");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _arm_exit_watchdog_on_shutdown_signal(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_arm_exit_watchdog_on_shutdown_signal");
    }
    return 0;
}
