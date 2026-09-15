/* gate: hermes_cli_goals_py___session_waiting — traced copy of goal_session_waiting */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int goal_session_waiting(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("goal_session_waiting");
    int result = goal_session_waiting(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("goal_session_waiting");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s goal_session_waiting <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
