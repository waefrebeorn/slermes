/* gate: agent_account_usage_py__redeem_codex_reset_credit — traced copy of agent_account_usage_redeem_codex_reset_credit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_account_usage_redeem_codex_reset_credit(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_account_usage_redeem_codex_reset_credit");
    int result = agent_account_usage_redeem_codex_reset_credit(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_account_usage_redeem_codex_reset_credit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_account_usage_redeem_codex_reset_credit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
