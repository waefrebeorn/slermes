/* gate: port__hermes_cli_curses_ui_py___reconcile_cursor — traced copy of curses_reconcile_cursor (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void curses_reconcile_cursor(json_t *filtered, int cursor, int *out_cursor, int *out_pos);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s curses_reconcile_cursor <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("curses_reconcile_cursor");
    int _o2 = 0;
    int _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    curses_reconcile_cursor(_j0, (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), &_o2, &_o3);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o2); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o3); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("curses_reconcile_cursor");
    }
    return 0;
}
