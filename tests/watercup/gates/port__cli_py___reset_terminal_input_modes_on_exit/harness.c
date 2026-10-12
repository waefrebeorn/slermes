/* gate: port__cli_py___reset_terminal_input_modes_on_exit — traced copy of _reset_terminal_input_modes_on_exit (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _reset_terminal_input_modes_on_exit(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _reset_terminal_input_modes_on_exit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_reset_terminal_input_modes_on_exit");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _reset_terminal_input_modes_on_exit(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_reset_terminal_input_modes_on_exit");
    }
    return 0;
}
