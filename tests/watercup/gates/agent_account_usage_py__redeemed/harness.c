/* gate: agent_account_usage_py__redeemed — traced copy of agent_account_usage_redeemed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_account_usage_redeemed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_account_usage_redeemed");
    int result = agent_account_usage_redeemed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_account_usage_redeemed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_account_usage_redeemed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
