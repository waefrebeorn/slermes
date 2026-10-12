/* gate: real__hermes_cli_macos_tcc_anchor_py___copy_alias — GAP sentinel for _copy_alias */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_copy_alias";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _copy_alias <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_copy_alias");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_copy_alias");
    }
    return 0;
}
