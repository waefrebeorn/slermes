/* gate: real__gateway_platforms_yuanbao_py__middleware_names — GAP sentinel for middleware_names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "middleware_names";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s middleware_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("middleware_names");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("middleware_names");
    }
    return 0;
}
