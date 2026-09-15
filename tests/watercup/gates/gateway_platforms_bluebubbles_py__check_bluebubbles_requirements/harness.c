/* gate: gateway_platforms_bluebubbles_py__check_bluebubbles_requirements — traced copy of bb_check_bluebubbles_requirements */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bb_check_bluebubbles_requirements(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bb_check_bluebubbles_requirements");
    int result = bb_check_bluebubbles_requirements(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bb_check_bluebubbles_requirements");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bb_check_bluebubbles_requirements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
