/* gate: real__tools_shell_heredoc_py___mask_simple_quotes — GAP sentinel for _mask_simple_quotes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_mask_simple_quotes";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _mask_simple_quotes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_mask_simple_quotes");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_mask_simple_quotes");
    }
    return 0;
}
