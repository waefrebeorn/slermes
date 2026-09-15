/* gate: agent_insights_py___get_model_usage — traced copy of agent_insights_u_get_model_usage */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_insights_u_get_model_usage(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_insights_u_get_model_usage");
    int result = agent_insights_u_get_model_usage(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_insights_u_get_model_usage");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_insights_u_get_model_usage <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
