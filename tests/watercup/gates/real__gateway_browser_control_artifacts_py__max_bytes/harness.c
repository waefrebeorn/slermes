/* gate: real__gateway_browser_control_artifacts_py__max_bytes — GAP sentinel for max_bytes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "max_bytes";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s max_bytes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("max_bytes");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("max_bytes");
    }
    return 0;
}
