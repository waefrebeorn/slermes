/* gate: real__agent_monitoring_cron_health_py___duration_ms — GAP sentinel for _duration_ms */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_duration_ms";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _duration_ms <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_duration_ms");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_duration_ms");
    }
    return 0;
}
