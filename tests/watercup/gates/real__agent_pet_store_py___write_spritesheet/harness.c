/* gate: real__agent_pet_store_py___write_spritesheet — GAP sentinel for _write_spritesheet */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_write_spritesheet";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _write_spritesheet <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_write_spritesheet");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_write_spritesheet");
    }
    return 0;
}
