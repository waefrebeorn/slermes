/* gate: real__tools_env_probe_py___plugin_backend_is_remote — GAP sentinel for _plugin_backend_is_remote */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_plugin_backend_is_remote";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _plugin_backend_is_remote <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_plugin_backend_is_remote");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_plugin_backend_is_remote");
    }
    return 0;
}
