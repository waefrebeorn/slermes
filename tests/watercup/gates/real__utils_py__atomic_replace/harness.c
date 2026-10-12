/* gate: real__utils_py__atomic_replace — GAP sentinel for atomic_replace */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "atomic_replace";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s atomic_replace <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("atomic_replace");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("atomic_replace");
    }
    return 0;
}
