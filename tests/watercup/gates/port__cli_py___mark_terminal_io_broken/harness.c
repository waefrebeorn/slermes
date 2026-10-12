/* gate: port__cli_py___mark_terminal_io_broken — GAP sentinel for _mark_terminal_io_broken */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_mark_terminal_io_broken";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _mark_terminal_io_broken <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_mark_terminal_io_broken");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_mark_terminal_io_broken");
    }
    return 0;
}
