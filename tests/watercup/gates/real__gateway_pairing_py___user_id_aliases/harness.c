/* gate: real__gateway_pairing_py___user_id_aliases — GAP sentinel for _user_id_aliases */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_user_id_aliases";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _user_id_aliases <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_user_id_aliases");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_user_id_aliases");
    }
    return 0;
}
