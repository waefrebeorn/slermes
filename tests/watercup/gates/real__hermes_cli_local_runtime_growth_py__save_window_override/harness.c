/* gate: real__hermes_cli_local_runtime_growth_py__save_window_override — GAP sentinel for save_window_override */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "save_window_override";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s save_window_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("save_window_override");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("save_window_override");
    }
    return 0;
}
