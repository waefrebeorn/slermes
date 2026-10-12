/* gate: real__gateway_drain_control_py___marker_is_stale — GAP sentinel for _marker_is_stale */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_marker_is_stale";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _marker_is_stale <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_marker_is_stale");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_marker_is_stale");
    }
    return 0;
}
