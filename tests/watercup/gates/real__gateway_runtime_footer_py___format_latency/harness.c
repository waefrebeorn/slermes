/* gate: real__gateway_runtime_footer_py___format_latency — GAP sentinel for _format_latency */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_format_latency";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _format_latency <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_format_latency");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_format_latency");
    }
    return 0;
}
