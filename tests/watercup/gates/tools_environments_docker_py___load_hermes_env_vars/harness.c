/* gate: tools_environments_docker_py___load_hermes_env_vars — traced copy of envd_u_load_hermes_env_vars */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envd_u_load_hermes_env_vars(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envd_u_load_hermes_env_vars");
    int result = envd_u_load_hermes_env_vars(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envd_u_load_hermes_env_vars");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envd_u_load_hermes_env_vars <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
