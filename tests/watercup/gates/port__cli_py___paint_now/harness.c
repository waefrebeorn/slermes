/* gate: port__cli_py___paint_now — GAP sentinel for _paint_now */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_paint_now";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _paint_now <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_paint_now");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_paint_now");
    }
    return 0;
}
