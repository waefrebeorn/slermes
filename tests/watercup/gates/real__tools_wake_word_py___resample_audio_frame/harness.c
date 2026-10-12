/* gate: real__tools_wake_word_py___resample_audio_frame — GAP sentinel for _resample_audio_frame */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_resample_audio_frame";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _resample_audio_frame <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_resample_audio_frame");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_resample_audio_frame");
    }
    return 0;
}
