/* gate: agent_credits_tracker_py__age_seconds — traced copy of agent_credits_tracker_age_seconds */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_credits_tracker_age_seconds(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_credits_tracker_age_seconds");
    int result = agent_credits_tracker_age_seconds(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_credits_tracker_age_seconds");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_credits_tracker_age_seconds <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
