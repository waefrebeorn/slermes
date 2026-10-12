/* gate: real__gateway_platforms_base_py___match_extensionless_path — GAP sentinel for _match_extensionless_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_match_extensionless_path";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _match_extensionless_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_match_extensionless_path");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_match_extensionless_path");
    }
    return 0;
}
