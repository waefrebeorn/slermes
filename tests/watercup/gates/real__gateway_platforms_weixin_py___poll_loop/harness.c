/* gate: real__gateway_platforms_weixin_py___poll_loop — GAP sentinel for _poll_loop */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_poll_loop";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _poll_loop <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_poll_loop");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_poll_loop");
    }
    return 0;
}
