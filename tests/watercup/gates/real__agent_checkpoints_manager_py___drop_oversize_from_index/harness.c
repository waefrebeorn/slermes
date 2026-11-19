/* gate: real__tools_checkpoint_manager_py___drop_oversize_from_index — GAP sentinel for _drop_oversize_from_index */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_drop_oversize_from_index";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _drop_oversize_from_index <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_drop_oversize_from_index");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_drop_oversize_from_index");
    }
    return 0;
}
