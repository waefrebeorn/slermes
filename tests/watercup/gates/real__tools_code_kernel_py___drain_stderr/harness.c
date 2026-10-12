/* gate: real__tools_code_kernel_py___drain_stderr — GAP sentinel for _drain_stderr */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_drain_stderr";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _drain_stderr <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_drain_stderr");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_drain_stderr");
    }
    return 0;
}
