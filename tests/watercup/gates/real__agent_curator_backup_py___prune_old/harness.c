/* gate: real__agent_curator_backup_py___prune_old — GAP sentinel for _prune_old */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_prune_old";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _prune_old <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_prune_old");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_prune_old");
    }
    return 0;
}
