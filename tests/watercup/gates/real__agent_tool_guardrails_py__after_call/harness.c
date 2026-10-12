/* gate: real__agent_tool_guardrails_py__after_call — GAP sentinel for after_call */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "after_call";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s after_call <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("after_call");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("after_call");
    }
    return 0;
}
