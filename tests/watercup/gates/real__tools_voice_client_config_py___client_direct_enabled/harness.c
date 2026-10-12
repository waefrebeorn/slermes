/* gate: real__tools_voice_client_config_py___client_direct_enabled — GAP sentinel for _client_direct_enabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_client_direct_enabled";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _client_direct_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_client_direct_enabled");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_client_direct_enabled");
    }
    return 0;
}
