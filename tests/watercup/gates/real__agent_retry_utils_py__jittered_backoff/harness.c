/* gate: real__agent_retry_utils_py__jittered_backoff — GAP sentinel for jittered_backoff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "jittered_backoff";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s jittered_backoff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("jittered_backoff");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("jittered_backoff");
    }
    return 0;
}
