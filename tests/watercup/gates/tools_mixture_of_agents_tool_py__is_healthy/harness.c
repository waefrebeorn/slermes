/* gate: tools_mixture_of_agents_tool_py__is_healthy — traced copy of moa_provider_health_is_healthy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int moa_provider_health_is_healthy(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("moa_provider_health_is_healthy");
    int result = moa_provider_health_is_healthy(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("moa_provider_health_is_healthy");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_provider_health_is_healthy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
