/* gate: real__hermes_cli_main_py___desktop_dist_exists — GAP sentinel for _desktop_dist_exists */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_desktop_dist_exists";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _desktop_dist_exists <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_desktop_dist_exists");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_desktop_dist_exists");
    }
    return 0;
}
