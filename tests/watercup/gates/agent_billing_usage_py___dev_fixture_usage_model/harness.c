/* gate: agent_billing_usage_py___dev_fixture_usage_model — traced copy of agent_billing_usage_u_dev_fixture_usage_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_billing_usage_u_dev_fixture_usage_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_billing_usage_u_dev_fixture_usage_model");
    int result = agent_billing_usage_u_dev_fixture_usage_model(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_billing_usage_u_dev_fixture_usage_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_billing_usage_u_dev_fixture_usage_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
