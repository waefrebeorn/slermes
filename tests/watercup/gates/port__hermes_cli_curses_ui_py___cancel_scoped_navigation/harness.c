/* gate: port__hermes_cli_curses_ui_py___cancel_scoped_navigation — traced copy of curses_ui_cancel_scoped_navigation (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void curses_ui_cancel_scoped_navigation(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s curses_ui_cancel_scoped_navigation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("curses_ui_cancel_scoped_navigation");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    curses_ui_cancel_scoped_navigation(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("curses_ui_cancel_scoped_navigation");
    }
    return 0;
}
