/* gate: port__hermes_cli_update_cmd_py___stash_apply_failed_only_on_existing_untracked — traced copy of uc_stash_apply_failed_only_on_existing_untracked (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool uc_stash_apply_failed_only_on_existing_untracked(const char *stderr_text);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_stash_apply_failed_only_on_existing_untracked <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_stash_apply_failed_only_on_existing_untracked");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = uc_stash_apply_failed_only_on_existing_untracked(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_stash_apply_failed_only_on_existing_untracked");
    }
    return 0;
}
