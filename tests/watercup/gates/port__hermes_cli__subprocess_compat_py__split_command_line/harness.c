/* gate: port__hermes_cli__subprocess_compat_py__split_command_line — traced copy of subprocess_compat_split_command_line (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * subprocess_compat_split_command_line(const char *line, int *err);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s subprocess_compat_split_command_line <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("subprocess_compat_split_command_line");
    int _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = subprocess_compat_split_command_line(argv[i], &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("subprocess_compat_split_command_line");
    }
    return 0;
}
