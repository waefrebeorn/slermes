/* gate: cron_lifecycle_guard_py___command_token_index — traced copy of cron_lifecycle_command_token_index */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_lifecycle_command_token_index(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_lifecycle_command_token_index");
    int result = cron_lifecycle_command_token_index(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_lifecycle_command_token_index");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_lifecycle_command_token_index <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
