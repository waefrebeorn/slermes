/* gate: tools_env_probe_py___ensure_probe_started — traced copy of tools_env_probe_u_ensure_probe_started */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_env_probe_u_ensure_probe_started(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_env_probe_u_ensure_probe_started");
    int result = tools_env_probe_u_ensure_probe_started(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_env_probe_u_ensure_probe_started");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_env_probe_u_ensure_probe_started <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
