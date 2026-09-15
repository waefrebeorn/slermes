/* gate: cron_scheduler_py___consume_interrupted_flag — traced copy of scheduler_consume_interrupted_flag */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool scheduler_consume_interrupted_flag(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("scheduler_consume_interrupted_flag");
    bool result = scheduler_consume_interrupted_flag(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scheduler_consume_interrupted_flag");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scheduler_consume_interrupted_flag <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
