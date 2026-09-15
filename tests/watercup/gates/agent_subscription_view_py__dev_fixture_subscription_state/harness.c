/* gate: agent_subscription_view_py__dev_fixture_subscription_state — traced copy of agent_subscription_view_dev_fixture_subscription_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_subscription_view_dev_fixture_subscription_state(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_subscription_view_dev_fixture_subscription_state");
    int result = agent_subscription_view_dev_fixture_subscription_state(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_subscription_view_dev_fixture_subscription_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_subscription_view_dev_fixture_subscription_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
