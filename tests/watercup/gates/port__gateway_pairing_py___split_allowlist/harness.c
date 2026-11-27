/* gate: port__gateway_pairing_py___split_allowlist — GAP sentinel for _split_allowlist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_split_allowlist";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _split_allowlist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_split_allowlist");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_split_allowlist");
    }
    return 0;
}
