/* gate: real__gateway_run_py___event_media_is_video — GAP sentinel for _event_media_is_video */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_event_media_is_video";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _event_media_is_video <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_event_media_is_video");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_event_media_is_video");
    }
    return 0;
}
