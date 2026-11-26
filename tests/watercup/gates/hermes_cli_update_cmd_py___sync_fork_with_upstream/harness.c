/* gate: hermes_cli_update_cmd_py___sync_fork_with_upstream — traced copy of uc_sync_fork_with_upstream */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uc_sync_fork_with_upstream(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("uc_sync_fork_with_upstream");
    int result = uc_sync_fork_with_upstream(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uc_sync_fork_with_upstream");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_sync_fork_with_upstream <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
