/* gate: real__agent_tool_executor_py___cancelled_tool_result — GAP sentinel for _cancelled_tool_result */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_cancelled_tool_result";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _cancelled_tool_result <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_cancelled_tool_result");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_cancelled_tool_result");
    }
    return 0;
}
