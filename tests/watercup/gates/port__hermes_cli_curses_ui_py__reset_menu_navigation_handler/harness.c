/* gate: port__hermes_cli_curses_ui_py__reset_menu_navigation_handler — traced copy of curses_ui_reset_menu_navigation_handler (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void curses_ui_reset_menu_navigation_handler(int token);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s curses_ui_reset_menu_navigation_handler <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("curses_ui_reset_menu_navigation_handler");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    curses_ui_reset_menu_navigation_handler(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("curses_ui_reset_menu_navigation_handler");
    }
    return 0;
}
