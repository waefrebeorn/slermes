/* gate: tools_env_probe_py__warm_environment_probe_async — traced copy of tools_env_probe_warm_environment_probe_async */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_env_probe_warm_environment_probe_async(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_env_probe_warm_environment_probe_async");
    int result = tools_env_probe_warm_environment_probe_async(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_env_probe_warm_environment_probe_async");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_env_probe_warm_environment_probe_async <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
