/* gate: real__hermes_cli__scan_venv_blockers_py___local_preview_metadata — GAP sentinel for _local_preview_metadata */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_local_preview_metadata";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _local_preview_metadata <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_local_preview_metadata");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_local_preview_metadata");
    }
    return 0;
}
