/* gate: real__agent_lsp_manager_py___reap_idle_once — GAP sentinel for _reap_idle_once */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_reap_idle_once";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _reap_idle_once <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_reap_idle_once");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_reap_idle_once");
    }
    return 0;
}
