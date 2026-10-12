/* gate: real__tools_image_generation_tool_py___confine_source_images — GAP sentinel for _confine_source_images */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_confine_source_images";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _confine_source_images <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_confine_source_images");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_confine_source_images");
    }
    return 0;
}
