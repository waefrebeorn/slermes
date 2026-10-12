/* gate: real__agent_copilot_acp_client_py___permission_denied — GAP sentinel for _permission_denied */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_permission_denied";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _permission_denied <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_permission_denied");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_permission_denied");
    }
    return 0;
}
