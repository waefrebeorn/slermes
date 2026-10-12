/* gate: real__agent_tool_dispatch_helpers_py___paths_overlap — GAP sentinel for _paths_overlap */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_paths_overlap";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _paths_overlap <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_paths_overlap");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_paths_overlap");
    }
    return 0;
}
