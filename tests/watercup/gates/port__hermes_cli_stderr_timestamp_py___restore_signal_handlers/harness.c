/* gate: port__hermes_cli_stderr_timestamp_py___restore_signal_handlers — traced copy of _restore_signal_handlers (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _restore_signal_handlers(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _restore_signal_handlers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_restore_signal_handlers");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _restore_signal_handlers(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_restore_signal_handlers");
    }
    return 0;
}
