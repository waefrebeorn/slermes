/* gate: agent_subscription_view_py__can_change_plan — traced copy of agent_subscription_view_can_change_plan */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_subscription_view_can_change_plan(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_subscription_view_can_change_plan");
    int result = agent_subscription_view_can_change_plan(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_subscription_view_can_change_plan");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_subscription_view_can_change_plan <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
