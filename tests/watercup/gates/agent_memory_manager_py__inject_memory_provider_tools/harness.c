/* gate: agent_memory_manager_py__inject_memory_provider_tools — traced copy of mm_inject_memory_provider_tools */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mm_inject_memory_provider_tools(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mm_inject_memory_provider_tools");
    int result = mm_inject_memory_provider_tools(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mm_inject_memory_provider_tools");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mm_inject_memory_provider_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
