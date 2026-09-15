/* gate: tools_environments_docker_py__reap_orphan_containers — traced copy of envd_reap_orphan_containers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envd_reap_orphan_containers(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envd_reap_orphan_containers");
    int result = envd_reap_orphan_containers(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envd_reap_orphan_containers");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envd_reap_orphan_containers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
