/* gate: real__hermes_state_py__logical_size_bytes — GAP sentinel for logical_size_bytes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "logical_size_bytes";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s logical_size_bytes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("logical_size_bytes");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("logical_size_bytes");
    }
    return 0;
}
