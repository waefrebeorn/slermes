/* gate: port__cli_py___force_full_redraw — GAP sentinel for _force_full_redraw */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_force_full_redraw";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _force_full_redraw <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_force_full_redraw");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_force_full_redraw");
    }
    return 0;
}
