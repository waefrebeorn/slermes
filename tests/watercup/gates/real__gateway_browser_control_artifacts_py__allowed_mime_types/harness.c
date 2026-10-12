/* gate: real__gateway_browser_control_artifacts_py__allowed_mime_types — GAP sentinel for allowed_mime_types */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "allowed_mime_types";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s allowed_mime_types <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("allowed_mime_types");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("allowed_mime_types");
    }
    return 0;
}
