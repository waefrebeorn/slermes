/* gate: hermes_cli_main_py___cleanup_pending_shim_renames — traced copy of mn_cleanup_pending_shim_renames */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mn_cleanup_pending_shim_renames(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mn_cleanup_pending_shim_renames");
    int result = mn_cleanup_pending_shim_renames(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mn_cleanup_pending_shim_renames");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mn_cleanup_pending_shim_renames <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
