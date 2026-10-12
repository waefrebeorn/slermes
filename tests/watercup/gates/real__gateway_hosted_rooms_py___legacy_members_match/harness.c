/* gate: real__gateway_hosted_rooms_py___legacy_members_match — GAP sentinel for _legacy_members_match */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_legacy_members_match";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _legacy_members_match <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_legacy_members_match");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_legacy_members_match");
    }
    return 0;
}
