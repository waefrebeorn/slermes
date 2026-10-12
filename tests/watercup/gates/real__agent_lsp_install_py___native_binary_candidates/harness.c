/* gate: real__agent_lsp_install_py___native_binary_candidates — GAP sentinel for _native_binary_candidates */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_native_binary_candidates";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _native_binary_candidates <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_native_binary_candidates");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_native_binary_candidates");
    }
    return 0;
}
