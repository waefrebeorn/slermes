/* gate: real__hermes_cli_main_py___tui_need_rebuild — GAP sentinel for _tui_need_rebuild */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_tui_need_rebuild";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _tui_need_rebuild <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_tui_need_rebuild");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_tui_need_rebuild");
    }
    return 0;
}
