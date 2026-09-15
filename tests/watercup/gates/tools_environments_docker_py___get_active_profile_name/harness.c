/* gate: tools_environments_docker_py___get_active_profile_name — traced copy of envd_u_get_active_profile_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envd_u_get_active_profile_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envd_u_get_active_profile_name");
    int result = envd_u_get_active_profile_name(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envd_u_get_active_profile_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envd_u_get_active_profile_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
