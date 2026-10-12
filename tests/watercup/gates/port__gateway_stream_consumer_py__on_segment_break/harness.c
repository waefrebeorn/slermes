/* gate: port__gateway_stream_consumer_py__on_segment_break — GAP sentinel for on_segment_break */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "on_segment_break";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s on_segment_break <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("on_segment_break");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("on_segment_break");
    }
    return 0;
}
