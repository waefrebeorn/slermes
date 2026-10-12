/* gate: real__tools_voice_mode_py__listen_for_speech — GAP sentinel for listen_for_speech */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "listen_for_speech";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s listen_for_speech <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("listen_for_speech");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("listen_for_speech");
    }
    return 0;
}
