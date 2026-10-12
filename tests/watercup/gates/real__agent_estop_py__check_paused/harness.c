/* gate: real__agent_estop_py__check_paused — GAP sentinel for check_paused */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "check_paused";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s check_paused <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("check_paused");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("check_paused");
    }
    return 0;
}
