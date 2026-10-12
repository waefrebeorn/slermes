/* gate: real__agent_lsp_install_py__try_install — GAP sentinel for try_install */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "try_install";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s try_install <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("try_install");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("try_install");
    }
    return 0;
}
