/* gate: real__agent_image_routing_py___supports_vision_override — GAP sentinel for _supports_vision_override */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_supports_vision_override";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _supports_vision_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_supports_vision_override");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_supports_vision_override");
    }
    return 0;
}
