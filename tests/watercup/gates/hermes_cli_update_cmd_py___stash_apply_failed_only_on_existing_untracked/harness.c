/* gate: hermes_cli_update_cmd_py___stash_apply_failed_only_on_existing_untracked — traced copy of uc_stash_apply_failed_only_on_existing_untracked */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool uc_stash_apply_failed_only_on_existing_untracked(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uc_stash_apply_failed_only_on_existing_untracked");
    bool result = uc_stash_apply_failed_only_on_existing_untracked(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uc_stash_apply_failed_only_on_existing_untracked");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_stash_apply_failed_only_on_existing_untracked <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
