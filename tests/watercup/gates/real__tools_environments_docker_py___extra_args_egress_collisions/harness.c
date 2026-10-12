/* gate: real__tools_environments_docker_py___extra_args_egress_collisions — GAP sentinel for _extra_args_egress_collisions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_extra_args_egress_collisions";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _extra_args_egress_collisions <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_extra_args_egress_collisions");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_extra_args_egress_collisions");
    }
    return 0;
}
