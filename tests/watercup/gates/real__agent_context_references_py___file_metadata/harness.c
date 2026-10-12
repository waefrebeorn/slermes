/* gate: real__agent_context_references_py___file_metadata — GAP sentinel for _file_metadata */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_file_metadata";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _file_metadata <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_file_metadata");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_file_metadata");
    }
    return 0;
}
