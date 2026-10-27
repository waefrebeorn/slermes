/* gate: port__cli_py___approval_callback — GAP sentinel for _approval_callback */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_approval_callback";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _approval_callback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_approval_callback");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_approval_callback");
    }
    return 0;
}
