/* gate: cron_executions_py___prune_unlocked — traced copy of cron_executions_u_prune_unlocked */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_executions_u_prune_unlocked(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_executions_u_prune_unlocked");
    int result = cron_executions_u_prune_unlocked(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_executions_u_prune_unlocked");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_executions_u_prune_unlocked <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
