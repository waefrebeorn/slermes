/* gate: real__gateway_stream_consumer_py___raw_message_limit — GAP sentinel for _raw_message_limit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_raw_message_limit";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _raw_message_limit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_raw_message_limit");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_raw_message_limit");
    }
    return 0;
}
