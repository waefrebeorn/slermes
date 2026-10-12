/* gate: port__cli_py___usage_reset — GAP sentinel for _usage_reset */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_usage_reset";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _usage_reset <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_usage_reset");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_usage_reset");
    }
    return 0;
}
