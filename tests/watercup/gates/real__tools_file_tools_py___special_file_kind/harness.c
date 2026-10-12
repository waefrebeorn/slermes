/* gate: real__tools_file_tools_py___special_file_kind — GAP sentinel for _special_file_kind */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_special_file_kind";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _special_file_kind <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_special_file_kind");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_special_file_kind");
    }
    return 0;
}
