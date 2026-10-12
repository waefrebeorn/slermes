/* gate: real__tools_read_extract_py___sheet_part — GAP sentinel for _sheet_part */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_sheet_part";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _sheet_part <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_sheet_part");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_sheet_part");
    }
    return 0;
}
