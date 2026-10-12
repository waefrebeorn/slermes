/* gate: real__gateway_platforms_media_cache_py__ext_for_mime — GAP sentinel for ext_for_mime */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "ext_for_mime";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ext_for_mime <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ext_for_mime");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ext_for_mime");
    }
    return 0;
}
