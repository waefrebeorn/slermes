/* gate: port__cli_py___normalize_git_bash_path — traced copy of cli_normalize_git_bash_path (sizet_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern size_t cli_normalize_git_bash_path(const char *s);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_normalize_git_bash_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_normalize_git_bash_path");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    size_t result = cli_normalize_git_bash_path((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%zu", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_normalize_git_bash_path");
    }
    return 0;
}
