/* gate: real__hermes_cli_journey_py___build_payload — GAP sentinel for _build_payload */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_build_payload";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _build_payload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_build_payload");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_build_payload");
    }
    return 0;
}
