/* gate: cron_scheduler_py___teardown_cron_agent — traced copy of cron_scheduler_u_teardown_cron_agent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_scheduler_u_teardown_cron_agent(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_scheduler_u_teardown_cron_agent");
    int result = cron_scheduler_u_teardown_cron_agent(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_scheduler_u_teardown_cron_agent");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_scheduler_u_teardown_cron_agent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
