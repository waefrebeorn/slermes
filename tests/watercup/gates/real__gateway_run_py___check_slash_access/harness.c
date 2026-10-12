/* gate: real__gateway_run_py___check_slash_access — GAP sentinel for _check_slash_access */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_check_slash_access";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _check_slash_access <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_check_slash_access");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_check_slash_access");
    }
    return 0;
}
