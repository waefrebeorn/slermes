/* gate: agent_memory_manager_py__memory_provider_tools_enabled — traced copy of mm_memory_provider_tools_enabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mm_memory_provider_tools_enabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mm_memory_provider_tools_enabled");
    int result = mm_memory_provider_tools_enabled(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mm_memory_provider_tools_enabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mm_memory_provider_tools_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
