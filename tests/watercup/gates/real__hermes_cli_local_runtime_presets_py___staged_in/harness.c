/* gate: real__hermes_cli_local_runtime_presets_py___staged_in — GAP sentinel for _staged_in */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_staged_in";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _staged_in <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_staged_in");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_staged_in");
    }
    return 0;
}
