/* gate: real__gateway_platforms_bluebubbles_py___api_url — GAP sentinel for _api_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_api_url";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _api_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_api_url");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_api_url");
    }
    return 0;
}
