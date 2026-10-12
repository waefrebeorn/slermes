/* gate: real__gateway_run_py___reconnect_backoff — GAP sentinel for _reconnect_backoff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_reconnect_backoff";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _reconnect_backoff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_reconnect_backoff");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_reconnect_backoff");
    }
    return 0;
}
