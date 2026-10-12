/* gate: real__hermes_cli_main_py___suppress_mouse_residue_early — GAP sentinel for _suppress_mouse_residue_early */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_suppress_mouse_residue_early";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _suppress_mouse_residue_early <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_suppress_mouse_residue_early");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_suppress_mouse_residue_early");
    }
    return 0;
}
