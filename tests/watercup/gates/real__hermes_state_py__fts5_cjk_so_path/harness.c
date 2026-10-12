/* gate: real__hermes_state_py__fts5_cjk_so_path — GAP sentinel for fts5_cjk_so_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "fts5_cjk_so_path";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fts5_cjk_so_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fts5_cjk_so_path");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fts5_cjk_so_path");
    }
    return 0;
}
