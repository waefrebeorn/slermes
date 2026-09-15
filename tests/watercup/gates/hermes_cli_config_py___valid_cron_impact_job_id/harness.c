/* gate: hermes_cli_config_py___valid_cron_impact_job_id — traced copy of _valid_cron_impact_job_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _valid_cron_impact_job_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_valid_cron_impact_job_id");
    int result = _valid_cron_impact_job_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_valid_cron_impact_job_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _valid_cron_impact_job_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
