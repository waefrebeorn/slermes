/* gate: cron_scheduler_py___is_known_delivery_platform — traced copy of scheduler_is_known_delivery_platform */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int scheduler_is_known_delivery_platform(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("scheduler_is_known_delivery_platform");
    int result = scheduler_is_known_delivery_platform(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scheduler_is_known_delivery_platform");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scheduler_is_known_delivery_platform <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
