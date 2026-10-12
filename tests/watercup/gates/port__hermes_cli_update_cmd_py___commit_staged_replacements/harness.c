/* gate: port__hermes_cli_update_cmd_py___commit_staged_replacements — traced copy of uc_commit_staged_replacements (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int uc_commit_staged_replacements(const char **staged_pairs);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_commit_staged_replacements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_commit_staged_replacements");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = uc_commit_staged_replacements(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_commit_staged_replacements");
    }
    return 0;
}
