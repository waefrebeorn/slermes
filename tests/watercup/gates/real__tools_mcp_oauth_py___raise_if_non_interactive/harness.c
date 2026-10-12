/* gate: real__tools_mcp_oauth_py___raise_if_non_interactive — GAP sentinel for _raise_if_non_interactive */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_raise_if_non_interactive";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _raise_if_non_interactive <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_raise_if_non_interactive");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_raise_if_non_interactive");
    }
    return 0;
}
