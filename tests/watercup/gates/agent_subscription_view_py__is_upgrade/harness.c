/* gate: agent_subscription_view_py__is_upgrade — traced copy of agent_subscription_view_is_upgrade */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_subscription_view_is_upgrade(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_subscription_view_is_upgrade");
    int result = agent_subscription_view_is_upgrade(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_subscription_view_is_upgrade");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_subscription_view_is_upgrade <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
