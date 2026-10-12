/* gate: real__gateway_run_py___non_conversational_metadata — GAP sentinel for _non_conversational_metadata */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_non_conversational_metadata";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _non_conversational_metadata <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_non_conversational_metadata");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_non_conversational_metadata");
    }
    return 0;
}
