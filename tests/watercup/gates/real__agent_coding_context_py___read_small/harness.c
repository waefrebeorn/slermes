/* gate: real__agent_coding_context_py___read_small — GAP sentinel for _read_small */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_read_small";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _read_small <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_read_small");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_read_small");
    }
    return 0;
}
