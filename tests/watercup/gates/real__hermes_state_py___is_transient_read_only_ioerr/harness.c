/* gate: real__hermes_state_py___is_transient_read_only_ioerr — GAP sentinel for hermes_state.py:_is_transient_read_only_ioerr */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static int gap_sentinel(const char *input) {
    (void)input;
    return 0;
}

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_transient_read_only_ioerr");
    int result = gap_sentinel(input);
#if 1
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", "_is_transient_read_only_ioerr");
    WCUP_VALSTR(buf);
#else
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", (int)result);
    WCUP_VALSTR(buf);
#endif
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_transient_read_only_ioerr");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_transient_read_only_ioerr <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
