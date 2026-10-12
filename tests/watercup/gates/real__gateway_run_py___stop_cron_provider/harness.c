/* gate: real__gateway_run_py___stop_cron_provider — GAP sentinel for _stop_cron_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_stop_cron_provider";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _stop_cron_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_stop_cron_provider");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_stop_cron_provider");
    }
    return 0;
}
