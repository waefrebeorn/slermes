/* gate: cron_jobs_py__get_ticker_last_error — traced copy of cron_jobs_get_ticker_last_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_jobs_get_ticker_last_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_jobs_get_ticker_last_error");
    int result = cron_jobs_get_ticker_last_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_jobs_get_ticker_last_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_jobs_get_ticker_last_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
