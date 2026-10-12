/* gate: real__tools_tirith_security_py___warn_once — GAP sentinel for _warn_once */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_warn_once";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _warn_once <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_warn_once");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_warn_once");
    }
    return 0;
}
