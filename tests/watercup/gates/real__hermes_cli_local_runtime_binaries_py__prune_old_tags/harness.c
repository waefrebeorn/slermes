/* gate: real__hermes_cli_local_runtime_binaries_py__prune_old_tags — GAP sentinel for prune_old_tags */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "prune_old_tags";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s prune_old_tags <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("prune_old_tags");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("prune_old_tags");
    }
    return 0;
}
