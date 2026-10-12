/* gate: port__hermes_cli_update_cmd_py___reject_unsafe_stash_restore — traced copy of _reject_unsafe_stash_restore (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _reject_unsafe_stash_restore(const char *git_cmd, const char *cwd, const char *stash_ref, const char *preexisting_untracked, const char *failing_target, const char *detail);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _reject_unsafe_stash_restore <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_reject_unsafe_stash_restore");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _reject_unsafe_stash_restore(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_reject_unsafe_stash_restore");
    }
    return 0;
}
