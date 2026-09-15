/* gate: hermes_cli_active_sessions_py__release_orphaned_leases — traced copy of activesess_release_orphaned_leases */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int activesess_release_orphaned_leases(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("activesess_release_orphaned_leases");
    int result = activesess_release_orphaned_leases(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("activesess_release_orphaned_leases");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s activesess_release_orphaned_leases <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
