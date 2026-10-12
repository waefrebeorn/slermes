/* gate: real__agent_secret_sources_onepassword_py__override_existing — GAP sentinel for override_existing */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "override_existing";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s override_existing <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("override_existing");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("override_existing");
    }
    return 0;
}
