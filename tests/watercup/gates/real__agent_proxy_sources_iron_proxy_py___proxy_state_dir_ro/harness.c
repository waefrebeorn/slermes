/* gate: real__agent_proxy_sources_iron_proxy_py___proxy_state_dir_ro — GAP sentinel for _proxy_state_dir_ro */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_proxy_state_dir_ro";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _proxy_state_dir_ro <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_proxy_state_dir_ro");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_proxy_state_dir_ro");
    }
    return 0;
}
