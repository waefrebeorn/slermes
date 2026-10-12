/* gate: real__gateway_platforms_base_py__send_animation — GAP sentinel for send_animation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "send_animation";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s send_animation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("send_animation");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("send_animation");
    }
    return 0;
}
