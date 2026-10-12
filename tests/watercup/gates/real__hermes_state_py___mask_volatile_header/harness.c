/* gate: real__hermes_state_py___mask_volatile_header — GAP sentinel for _mask_volatile_header */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_mask_volatile_header";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _mask_volatile_header <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_mask_volatile_header");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_mask_volatile_header");
    }
    return 0;
}
