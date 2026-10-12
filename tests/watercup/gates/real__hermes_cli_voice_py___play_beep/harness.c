/* gate: real__hermes_cli_voice_py___play_beep — GAP sentinel for _play_beep */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_play_beep";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _play_beep <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_play_beep");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_play_beep");
    }
    return 0;
}
