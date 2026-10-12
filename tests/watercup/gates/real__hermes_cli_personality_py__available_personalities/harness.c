/* gate: real__hermes_cli_personality_py__available_personalities — GAP sentinel for available_personalities */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "available_personalities";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s available_personalities <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("available_personalities");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("available_personalities");
    }
    return 0;
}
