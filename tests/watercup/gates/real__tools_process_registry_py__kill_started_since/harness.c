/* gate: real__tools_process_registry_py__kill_started_since — GAP sentinel for kill_started_since */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "kill_started_since";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kill_started_since <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kill_started_since");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kill_started_since");
    }
    return 0;
}
