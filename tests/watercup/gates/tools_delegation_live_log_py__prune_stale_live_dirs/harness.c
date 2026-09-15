/* gate: tools_delegation_live_log_py__prune_stale_live_dirs — traced copy of tools_delegation_live_log_prune_stale_live_dirs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_delegation_live_log_prune_stale_live_dirs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_delegation_live_log_prune_stale_live_dirs");
    int result = tools_delegation_live_log_prune_stale_live_dirs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_delegation_live_log_prune_stale_live_dirs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_delegation_live_log_prune_stale_live_dirs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
