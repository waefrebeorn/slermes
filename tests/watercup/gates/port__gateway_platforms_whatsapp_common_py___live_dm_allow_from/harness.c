/* gate: port__gateway_platforms_whatsapp_common_py___live_dm_allow_from — GAP sentinel for _live_dm_allow_from */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_live_dm_allow_from";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _live_dm_allow_from <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_live_dm_allow_from");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_live_dm_allow_from");
    }
    return 0;
}
