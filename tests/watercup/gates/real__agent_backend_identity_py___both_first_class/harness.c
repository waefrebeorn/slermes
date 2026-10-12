/* gate: real__agent_backend_identity_py___both_first_class — GAP sentinel for _both_first_class */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_both_first_class";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _both_first_class <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_both_first_class");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_both_first_class");
    }
    return 0;
}
