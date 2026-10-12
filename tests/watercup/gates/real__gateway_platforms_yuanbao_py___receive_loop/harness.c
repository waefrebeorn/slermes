/* gate: real__gateway_platforms_yuanbao_py___receive_loop — GAP sentinel for _receive_loop */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_receive_loop";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _receive_loop <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_receive_loop");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_receive_loop");
    }
    return 0;
}
