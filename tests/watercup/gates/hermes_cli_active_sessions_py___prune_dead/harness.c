/* gate: hermes_cli_active_sessions_py___prune_dead — traced copy of hermes_cli_active_sessions_u_prune_dead */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_active_sessions_u_prune_dead(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_active_sessions_u_prune_dead");
    int result = hermes_cli_active_sessions_u_prune_dead(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_active_sessions_u_prune_dead");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_active_sessions_u_prune_dead <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
