/* gate: real__cli_py___pet_stop_anim — GAP sentinel for _pet_stop_anim */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_pet_stop_anim";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _pet_stop_anim <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_pet_stop_anim");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_pet_stop_anim");
    }
    return 0;
}
