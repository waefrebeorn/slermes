/* gate: real__gateway_streaming_tts_consumer_py__suppress_whole_file — GAP sentinel for suppress_whole_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "suppress_whole_file";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s suppress_whole_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("suppress_whole_file");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("suppress_whole_file");
    }
    return 0;
}
