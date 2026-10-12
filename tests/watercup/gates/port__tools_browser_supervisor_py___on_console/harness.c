/* gate: port__tools_browser_supervisor_py___on_console — traced copy of browser_supervisor_on_console_with_level (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void browser_supervisor_on_console_with_level(const char *text, const char *level);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_supervisor_on_console_with_level <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("browser_supervisor_on_console_with_level");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    browser_supervisor_on_console_with_level(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("browser_supervisor_on_console_with_level");
    }
    return 0;
}
