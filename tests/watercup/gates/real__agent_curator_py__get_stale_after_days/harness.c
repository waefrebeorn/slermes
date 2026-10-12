/* gate: real__agent_curator_py__get_stale_after_days — GAP sentinel for get_stale_after_days */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "get_stale_after_days";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_stale_after_days <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("get_stale_after_days");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("get_stale_after_days");
    }
    return 0;
}
