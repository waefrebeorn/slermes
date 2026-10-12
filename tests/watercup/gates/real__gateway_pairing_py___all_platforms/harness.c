/* gate: real__gateway_pairing_py___all_platforms — GAP sentinel for _all_platforms */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_all_platforms";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _all_platforms <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_all_platforms");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_all_platforms");
    }
    return 0;
}
