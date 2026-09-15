/* gate: agent_jiter_preload_py__preload_jiter_native_extension — traced copy of agent_jiter_preload_preload_jiter_native_extension */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_jiter_preload_preload_jiter_native_extension(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_jiter_preload_preload_jiter_native_extension");
    int result = agent_jiter_preload_preload_jiter_native_extension(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_jiter_preload_preload_jiter_native_extension");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_jiter_preload_preload_jiter_native_extension <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
