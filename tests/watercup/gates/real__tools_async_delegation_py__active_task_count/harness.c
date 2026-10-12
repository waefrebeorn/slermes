/* gate: real__tools_async_delegation_py__active_task_count — GAP sentinel for active_task_count */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "active_task_count";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s active_task_count <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("active_task_count");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("active_task_count");
    }
    return 0;
}
