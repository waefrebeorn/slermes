/* gate: cron_suggestions_py___set_status — traced copy of cron_sugg_set_status */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_sugg_set_status(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_sugg_set_status");
    int result = cron_sugg_set_status(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_sugg_set_status");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_sugg_set_status <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
