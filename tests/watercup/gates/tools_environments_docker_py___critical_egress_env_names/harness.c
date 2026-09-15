/* gate: tools_environments_docker_py___critical_egress_env_names — traced copy of envd_u_critical_egress_env_names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envd_u_critical_egress_env_names(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envd_u_critical_egress_env_names");
    int result = envd_u_critical_egress_env_names(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envd_u_critical_egress_env_names");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envd_u_critical_egress_env_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
