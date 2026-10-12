/* gate: real__tools_bot_mode_probe_py___roster_lines — GAP sentinel for _roster_lines */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_roster_lines";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _roster_lines <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_roster_lines");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_roster_lines");
    }
    return 0;
}
