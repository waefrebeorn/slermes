/* gate: agent_subscription_view_py___dev_current — traced copy of agent_subscription_view_u_dev_current */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_subscription_view_u_dev_current(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_subscription_view_u_dev_current");
    int result = agent_subscription_view_u_dev_current(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_subscription_view_u_dev_current");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_subscription_view_u_dev_current <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
