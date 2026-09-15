/* gate: tools_env_probe_py___probe_worker — traced copy of tools_env_probe_u_probe_worker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_env_probe_u_probe_worker(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_env_probe_u_probe_worker");
    int result = tools_env_probe_u_probe_worker(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_env_probe_u_probe_worker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_env_probe_u_probe_worker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
