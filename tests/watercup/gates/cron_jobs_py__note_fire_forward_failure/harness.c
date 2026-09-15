/* gate: cron_jobs_py__note_fire_forward_failure — traced copy of cronjobs_note_fire_forward_failure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cronjobs_note_fire_forward_failure(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cronjobs_note_fire_forward_failure");
    int result = cronjobs_note_fire_forward_failure(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cronjobs_note_fire_forward_failure");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjobs_note_fire_forward_failure <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
