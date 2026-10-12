/* gate: real__gateway_run_py___voice_key — GAP sentinel for _voice_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_voice_key";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _voice_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_voice_key");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_voice_key");
    }
    return 0;
}
