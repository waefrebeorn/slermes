/* gate: port__agent_pet_render_py___crop_frames_to_alpha_union — GAP sentinel for _crop_frames_to_alpha_union */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_crop_frames_to_alpha_union";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _crop_frames_to_alpha_union <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_crop_frames_to_alpha_union");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_crop_frames_to_alpha_union");
    }
    return 0;
}
