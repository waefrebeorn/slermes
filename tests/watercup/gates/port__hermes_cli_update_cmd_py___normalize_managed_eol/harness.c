/* gate: port__hermes_cli_update_cmd_py___normalize_managed_eol — traced copy of uc_normalize_managed_eol (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int uc_normalize_managed_eol(const char *git_cmd[], const char *repo_root);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_normalize_managed_eol <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_normalize_managed_eol");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = uc_normalize_managed_eol(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_normalize_managed_eol");
    }
    return 0;
}
