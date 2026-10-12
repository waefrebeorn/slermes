/* gate: real__hermes_state_py___db_has_legacy_inline_fts — GAP sentinel for _db_has_legacy_inline_fts */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_db_has_legacy_inline_fts";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _db_has_legacy_inline_fts <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_db_has_legacy_inline_fts");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_db_has_legacy_inline_fts");
    }
    return 0;
}
