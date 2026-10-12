/* gate: real__gateway_session_state_py___present_nonzero — GAP sentinel for _present_nonzero */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_present_nonzero";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _present_nonzero <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_present_nonzero");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_present_nonzero");
    }
    return 0;
}
