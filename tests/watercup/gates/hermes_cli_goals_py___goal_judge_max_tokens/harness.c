/* gate: hermes_cli_goals_py___goal_judge_max_tokens — traced copy of goal_judge_max_tokens_resolved */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int goal_judge_max_tokens_resolved(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("goal_judge_max_tokens_resolved");
    int result = goal_judge_max_tokens_resolved(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("goal_judge_max_tokens_resolved");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s goal_judge_max_tokens_resolved <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
