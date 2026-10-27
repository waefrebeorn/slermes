/* gate: port__gateway_stream_consumer_py___visible_prefix — GAP sentinel for _visible_prefix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_visible_prefix";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _visible_prefix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_visible_prefix");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_visible_prefix");
    }
    return 0;
}
