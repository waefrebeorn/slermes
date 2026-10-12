/* gate: real__agent_lsp_install_py___install_pip — GAP sentinel for _install_pip */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_install_pip";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _install_pip <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_install_pip");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_install_pip");
    }
    return 0;
}
