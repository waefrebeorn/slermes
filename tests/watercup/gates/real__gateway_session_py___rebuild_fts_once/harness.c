/* gate: real__gateway_session_py___rebuild_fts_once — GAP sentinel for _rebuild_fts_once */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_rebuild_fts_once";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _rebuild_fts_once <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_rebuild_fts_once");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_rebuild_fts_once");
    }
    return 0;
}
