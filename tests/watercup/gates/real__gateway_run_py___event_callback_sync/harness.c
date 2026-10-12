/* gate: real__gateway_run_py___event_callback_sync — GAP sentinel for _event_callback_sync */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_event_callback_sync";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _event_callback_sync <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_event_callback_sync");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_event_callback_sync");
    }
    return 0;
}
