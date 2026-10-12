/* gate: real__hermes_cli_main_py___find_bundled_tui — GAP sentinel for _find_bundled_tui */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_find_bundled_tui";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _find_bundled_tui <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_find_bundled_tui");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_find_bundled_tui");
    }
    return 0;
}
