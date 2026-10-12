/* gate: real__tools_mcp_tool_py___prepend_path — GAP sentinel for _prepend_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_prepend_path";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _prepend_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_prepend_path");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_prepend_path");
    }
    return 0;
}
