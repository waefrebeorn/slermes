/* gate: real__agent_markdown_tables_py___disp_width — GAP sentinel for _disp_width */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_disp_width";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _disp_width <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_disp_width");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_disp_width");
    }
    return 0;
}
