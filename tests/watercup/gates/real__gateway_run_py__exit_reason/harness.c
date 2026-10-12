/* gate: real__gateway_run_py__exit_reason — GAP sentinel for exit_reason */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "exit_reason";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s exit_reason <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("exit_reason");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("exit_reason");
    }
    return 0;
}
