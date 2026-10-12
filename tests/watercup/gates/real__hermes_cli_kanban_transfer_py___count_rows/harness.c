/* gate: real__hermes_cli_kanban_transfer_py___count_rows — GAP sentinel for _count_rows */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_count_rows";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _count_rows <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_count_rows");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_count_rows");
    }
    return 0;
}
