/* gate: cron_scheduler_py___interpreter_shutting_down — traced copy of scheduler_interpreter_shutting_down */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool scheduler_interpreter_shutting_down(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("scheduler_interpreter_shutting_down");
    bool result = scheduler_interpreter_shutting_down(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scheduler_interpreter_shutting_down");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scheduler_interpreter_shutting_down <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
