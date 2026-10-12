/* gate: real__gateway_stream_consumer_py___compose_frame_content — GAP sentinel for _compose_frame_content */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_compose_frame_content";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _compose_frame_content <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_compose_frame_content");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_compose_frame_content");
    }
    return 0;
}
