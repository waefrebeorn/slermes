/* gate: port__hermes_cli_update_cmd_py___discard_stashed_changes — traced copy of uc_discard_stashed_changes (bool_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool uc_discard_stashed_changes(const char *git_cmd[], const char *cwd,
                                const char *stash_ref);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_discard_stashed_changes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_discard_stashed_changes");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = uc_discard_stashed_changes((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_discard_stashed_changes");
    }
    return 0;
}
