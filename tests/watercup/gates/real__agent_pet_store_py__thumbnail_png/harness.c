/* gate: real__agent_pet_store_py__thumbnail_png — GAP sentinel for thumbnail_png */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "thumbnail_png";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s thumbnail_png <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("thumbnail_png");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("thumbnail_png");
    }
    return 0;
}
