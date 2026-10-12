/* gate: real__gateway_platforms_base_py___media_delivery_allowed_roots — GAP sentinel for _media_delivery_allowed_roots */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_media_delivery_allowed_roots";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _media_delivery_allowed_roots <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_media_delivery_allowed_roots");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_media_delivery_allowed_roots");
    }
    return 0;
}
