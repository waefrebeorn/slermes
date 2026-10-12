/* gate: real__hermes_cli_main_py___probe_container — GAP sentinel for _probe_container */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_probe_container";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _probe_container <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_probe_container");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_probe_container");
    }
    return 0;
}
