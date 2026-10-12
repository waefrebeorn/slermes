/* gate: real__tools_tirith_security_py__ensure_installed — GAP sentinel for ensure_installed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "ensure_installed";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ensure_installed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ensure_installed");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ensure_installed");
    }
    return 0;
}
