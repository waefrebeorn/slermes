/* gate: agent_agent_runtime_helpers_py__cache_ttl_means_disabled — traced copy of arh_cache_ttl_means_disabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int arh_cache_ttl_means_disabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("arh_cache_ttl_means_disabled");
    int result = arh_cache_ttl_means_disabled(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("arh_cache_ttl_means_disabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s arh_cache_ttl_means_disabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
