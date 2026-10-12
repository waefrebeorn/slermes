/* gate: real__agent_secret_sources_registry_py__list_sources — GAP sentinel for list_sources */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "list_sources";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s list_sources <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("list_sources");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("list_sources");
    }
    return 0;
}
