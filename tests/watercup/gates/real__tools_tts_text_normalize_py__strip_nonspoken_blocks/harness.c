/* gate: real__tools_tts_text_normalize_py__strip_nonspoken_blocks — GAP sentinel for strip_nonspoken_blocks */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "strip_nonspoken_blocks";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s strip_nonspoken_blocks <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("strip_nonspoken_blocks");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("strip_nonspoken_blocks");
    }
    return 0;
}
