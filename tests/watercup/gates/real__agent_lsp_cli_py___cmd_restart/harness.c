/* gate: real__agent_lsp_cli_py___cmd_restart — GAP sentinel for _cmd_restart */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_cmd_restart";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _cmd_restart <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_cmd_restart");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_cmd_restart");
    }
    return 0;
}
