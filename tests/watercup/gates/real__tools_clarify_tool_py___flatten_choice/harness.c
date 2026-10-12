/* gate: real__tools_clarify_tool_py___flatten_choice — GAP sentinel for _flatten_choice */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_flatten_choice";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _flatten_choice <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_flatten_choice");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_flatten_choice");
    }
    return 0;
}
