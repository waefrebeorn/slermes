/* gate: real__tools_bot_mode_dm_py___spawn_delivery — GAP sentinel for _spawn_delivery */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_spawn_delivery";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _spawn_delivery <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_spawn_delivery");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_spawn_delivery");
    }
    return 0;
}
