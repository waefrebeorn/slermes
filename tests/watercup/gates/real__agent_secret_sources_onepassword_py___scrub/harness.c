/* gate: real__agent_secret_sources_onepassword_py___scrub — GAP sentinel for _scrub */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_scrub";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _scrub <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_scrub");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_scrub");
    }
    return 0;
}
