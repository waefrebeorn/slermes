/* gate: real__hermes_state_py__rewind_to_message — GAP sentinel for rewind_to_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "rewind_to_message";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rewind_to_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("rewind_to_message");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("rewind_to_message");
    }
    return 0;
}
