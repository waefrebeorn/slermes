/* gate: agent_oneshot_py__run_oneshot — traced copy of agent_oneshot_run_oneshot */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_oneshot_run_oneshot(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_oneshot_run_oneshot");
    int result = agent_oneshot_run_oneshot(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_oneshot_run_oneshot");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_oneshot_run_oneshot <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
