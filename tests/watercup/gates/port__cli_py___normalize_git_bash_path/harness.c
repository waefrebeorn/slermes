/* gate: port__cli_py___normalize_git_bash_path — traced copy of cli_normalize_git_bash_path (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern char * cli_normalize_git_bash_path(const char *p);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_normalize_git_bash_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_normalize_git_bash_path");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = cli_normalize_git_bash_path(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_normalize_git_bash_path");
    }
    return 0;
}
