/* gate: agent_subscription_view_py___format_dollars_grouped — traced copy of agent_subscription_view_u_format_dollars_grouped */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_subscription_view_u_format_dollars_grouped(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_subscription_view_u_format_dollars_grouped");
    int result = agent_subscription_view_u_format_dollars_grouped(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_subscription_view_u_format_dollars_grouped");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_subscription_view_u_format_dollars_grouped <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
