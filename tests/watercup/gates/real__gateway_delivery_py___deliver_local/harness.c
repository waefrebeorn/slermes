/* gate: real__gateway_delivery_py___deliver_local — GAP sentinel for _deliver_local */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_deliver_local";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _deliver_local <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_deliver_local");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_deliver_local");
    }
    return 0;
}
