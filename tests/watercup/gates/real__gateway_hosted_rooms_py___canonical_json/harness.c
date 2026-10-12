/* gate: real__gateway_hosted_rooms_py___canonical_json — GAP sentinel for _canonical_json */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_canonical_json";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _canonical_json <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_canonical_json");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_canonical_json");
    }
    return 0;
}
