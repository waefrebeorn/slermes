/* gate: real__agent_reasoning_timeouts_py__get_reasoning_stale_timeout_floor — GAP sentinel for agent/reasoning_timeouts.py:get_reasoning_stale_timeout_floor */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static int gap_sentinel(const char *input) {
    (void)input;
    return 0;
}

static void trace_one(const char *input) {
    WCUP_BEGIN("get_reasoning_stale_timeout_floor");
    int result = gap_sentinel(input);
#if 1
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", "get_reasoning_stale_timeout_floor");
    WCUP_VALSTR(buf);
#else
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", (int)result);
    WCUP_VALSTR(buf);
#endif
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("get_reasoning_stale_timeout_floor");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_reasoning_stale_timeout_floor <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
