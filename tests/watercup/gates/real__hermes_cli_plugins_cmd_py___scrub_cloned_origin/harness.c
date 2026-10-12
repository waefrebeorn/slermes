/* gate: real__hermes_cli_plugins_cmd_py___scrub_cloned_origin — GAP sentinel for _scrub_cloned_origin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_scrub_cloned_origin";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _scrub_cloned_origin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_scrub_cloned_origin");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_scrub_cloned_origin");
    }
    return 0;
}
