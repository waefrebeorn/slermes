/* gate: real__hermes_cli_nous_account_py___portal_base_url — GAP sentinel for _portal_base_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_portal_base_url";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _portal_base_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_portal_base_url");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_portal_base_url");
    }
    return 0;
}
