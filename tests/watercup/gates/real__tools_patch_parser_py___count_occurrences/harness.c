/* gate: real__tools_patch_parser_py___count_occurrences — GAP sentinel for _count_occurrences */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_count_occurrences";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _count_occurrences <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_count_occurrences");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_count_occurrences");
    }
    return 0;
}
