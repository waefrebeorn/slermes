/* gate: cron_jobs_py__parse_duration — traced copy of jobs_parse_duration */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int jobs_parse_duration(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("jobs_parse_duration");
    int result = jobs_parse_duration(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("jobs_parse_duration");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s jobs_parse_duration <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
