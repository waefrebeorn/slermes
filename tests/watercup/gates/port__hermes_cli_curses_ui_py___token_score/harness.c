/* gate: port__hermes_cli_curses_ui_py___token_score — traced copy of curses_token_score (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double curses_token_score(const char *orig, const char *lower, const char *token, int *matched);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s curses_token_score <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("curses_token_score");
    int _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = curses_token_score(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], &_o3);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o3); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("curses_token_score");
    }
    return 0;
}
