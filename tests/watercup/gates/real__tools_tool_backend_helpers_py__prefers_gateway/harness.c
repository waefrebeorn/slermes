/* gate: real__tools_tool_backend_helpers_py__prefers_gateway — GAP sentinel for prefers_gateway */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "prefers_gateway";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s prefers_gateway <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("prefers_gateway");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("prefers_gateway");
    }
    return 0;
}
