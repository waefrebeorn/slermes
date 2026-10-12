/* gate: real__agent_context_references_py___rg_files — GAP sentinel for _rg_files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_rg_files";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _rg_files <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_rg_files");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_rg_files");
    }
    return 0;
}
