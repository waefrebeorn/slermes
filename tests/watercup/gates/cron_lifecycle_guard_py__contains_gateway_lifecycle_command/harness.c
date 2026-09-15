/* gate: cron_lifecycle_guard_py__contains_gateway_lifecycle_command — traced copy of cron_lifecycle_contains_gateway_lifecycle_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cron_lifecycle_contains_gateway_lifecycle_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_lifecycle_contains_gateway_lifecycle_command");
    bool result = cron_lifecycle_contains_gateway_lifecycle_command(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_lifecycle_contains_gateway_lifecycle_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_lifecycle_contains_gateway_lifecycle_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
