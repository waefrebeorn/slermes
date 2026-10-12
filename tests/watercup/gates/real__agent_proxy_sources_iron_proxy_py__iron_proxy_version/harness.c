/* gate: real__agent_proxy_sources_iron_proxy_py__iron_proxy_version — GAP sentinel for iron_proxy_version */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "iron_proxy_version";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s iron_proxy_version <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("iron_proxy_version");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("iron_proxy_version");
    }
    return 0;
}
