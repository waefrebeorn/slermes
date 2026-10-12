/* gate: real__tools_terminal_tool_py___sudo_nopasswd_works — GAP sentinel for _sudo_nopasswd_works */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_sudo_nopasswd_works";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _sudo_nopasswd_works <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_sudo_nopasswd_works");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_sudo_nopasswd_works");
    }
    return 0;
}
