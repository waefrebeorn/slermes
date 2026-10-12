/* gate: real__agent_coding_context_py___coding_instructions — GAP sentinel for _coding_instructions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_coding_instructions";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _coding_instructions <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_coding_instructions");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_coding_instructions");
    }
    return 0;
}
