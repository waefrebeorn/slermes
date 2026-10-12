/* gate: real__tools_delegate_tool_py___strip_blocked_tools — GAP sentinel for _strip_blocked_tools */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_strip_blocked_tools";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _strip_blocked_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_strip_blocked_tools");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_strip_blocked_tools");
    }
    return 0;
}
