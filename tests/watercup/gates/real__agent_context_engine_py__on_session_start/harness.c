/* gate: real__agent_context_engine_py__on_session_start — GAP sentinel for on_session_start */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "on_session_start";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s on_session_start <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("on_session_start");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("on_session_start");
    }
    return 0;
}
