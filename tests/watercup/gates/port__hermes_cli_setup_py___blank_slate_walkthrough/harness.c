/* gate: port__hermes_cli_setup_py___blank_slate_walkthrough — traced copy of _blank_slate_walkthrough (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _blank_slate_walkthrough(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _blank_slate_walkthrough <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_blank_slate_walkthrough");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _blank_slate_walkthrough();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_blank_slate_walkthrough");
    }
    return 0;
}
