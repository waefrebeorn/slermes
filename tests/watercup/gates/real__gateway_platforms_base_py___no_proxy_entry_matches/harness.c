/* gate: real__gateway_platforms_base_py___no_proxy_entry_matches — GAP sentinel for _no_proxy_entry_matches */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_no_proxy_entry_matches";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _no_proxy_entry_matches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_no_proxy_entry_matches");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_no_proxy_entry_matches");
    }
    return 0;
}
