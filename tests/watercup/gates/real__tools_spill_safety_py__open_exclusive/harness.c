/* gate: real__tools_spill_safety_py__open_exclusive — GAP sentinel for open_exclusive */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "open_exclusive";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s open_exclusive <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("open_exclusive");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("open_exclusive");
    }
    return 0;
}
