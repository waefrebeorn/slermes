/* gate: cron_scheduler_provider_py___start_multiplex — traced copy of cron_scheduler_provider_u_start_multiplex */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_scheduler_provider_u_start_multiplex(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_scheduler_provider_u_start_multiplex");
    int result = cron_scheduler_provider_u_start_multiplex(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_scheduler_provider_u_start_multiplex");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_scheduler_provider_u_start_multiplex <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
