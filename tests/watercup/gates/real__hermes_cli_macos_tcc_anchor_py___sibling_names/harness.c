/* gate: real__hermes_cli_macos_tcc_anchor_py___sibling_names — GAP sentinel for _sibling_names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_sibling_names";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _sibling_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_sibling_names");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_sibling_names");
    }
    return 0;
}
