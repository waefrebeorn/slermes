/* gate: real__agent_pet_store_py__installed_pets — GAP sentinel for installed_pets */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "installed_pets";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s installed_pets <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("installed_pets");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("installed_pets");
    }
    return 0;
}
