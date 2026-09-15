/* gate: cron_scheduler_py___parse_wake_gate — traced copy of scheduler_parse_wake_gate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool scheduler_parse_wake_gate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("scheduler_parse_wake_gate");
    bool result = scheduler_parse_wake_gate(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scheduler_parse_wake_gate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scheduler_parse_wake_gate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
