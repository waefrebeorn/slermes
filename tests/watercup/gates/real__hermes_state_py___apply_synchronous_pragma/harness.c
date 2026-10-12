/* gate: real__hermes_state_py___apply_synchronous_pragma — GAP sentinel for _apply_synchronous_pragma */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_apply_synchronous_pragma";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _apply_synchronous_pragma <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_apply_synchronous_pragma");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_apply_synchronous_pragma");
    }
    return 0;
}
