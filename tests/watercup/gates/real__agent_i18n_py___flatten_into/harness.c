/* gate: real__agent_i18n_py___flatten_into — GAP sentinel for _flatten_into */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_flatten_into";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _flatten_into <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_flatten_into");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_flatten_into");
    }
    return 0;
}
