/* gate: agent_web_search_provider_py__get_provider_env — traced copy of agent_web_search_provider_get_provider_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_web_search_provider_get_provider_env(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_web_search_provider_get_provider_env");
    int result = agent_web_search_provider_get_provider_env(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_web_search_provider_get_provider_env");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_web_search_provider_get_provider_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
