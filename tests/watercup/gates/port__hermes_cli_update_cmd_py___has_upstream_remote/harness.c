/* gate: port__hermes_cli_update_cmd_py___has_upstream_remote — traced copy of uc_has_upstream_remote (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool uc_has_upstream_remote(const char *git_cmd[], const char *cwd);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_has_upstream_remote <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_has_upstream_remote");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = uc_has_upstream_remote(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_has_upstream_remote");
    }
    return 0;
}
