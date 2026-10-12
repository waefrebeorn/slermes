/* gate: real__cron_jobs_py___epoch_file_age — GAP sentinel for _epoch_file_age */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_epoch_file_age";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _epoch_file_age <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_epoch_file_age");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_epoch_file_age");
    }
    return 0;
}
