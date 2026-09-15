/* gate: tools_environments_docker_py__wait_for_cleanup — traced copy of envd_wait_for_cleanup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envd_wait_for_cleanup(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envd_wait_for_cleanup");
    int result = envd_wait_for_cleanup(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envd_wait_for_cleanup");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envd_wait_for_cleanup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
