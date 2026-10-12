/* gate: real__gateway_platforms_base_py___path_lacks_deliverable_extension — GAP sentinel for _path_lacks_deliverable_extension */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_path_lacks_deliverable_extension";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _path_lacks_deliverable_extension <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_path_lacks_deliverable_extension");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_path_lacks_deliverable_extension");
    }
    return 0;
}
