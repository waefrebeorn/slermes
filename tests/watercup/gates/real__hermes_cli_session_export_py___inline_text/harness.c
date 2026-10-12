/* gate: real__hermes_cli_session_export_py___inline_text — GAP sentinel for _inline_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_inline_text";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _inline_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_inline_text");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_inline_text");
    }
    return 0;
}
