/* gate: port__hermes_state_py__sanitize_title — traced copy of session_title_sanitize (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * session_title_sanitize(const char *title, bool *invalid);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_title_sanitize <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("session_title_sanitize");
    bool _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = session_title_sanitize(argv[i], &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    WCUP_VALSTR(_o1 ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("session_title_sanitize");
    }
    return 0;
}
