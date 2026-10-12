/* gate: real__tools_process_registry_py__spawn_via_env — GAP sentinel for spawn_via_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "spawn_via_env";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s spawn_via_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("spawn_via_env");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("spawn_via_env");
    }
    return 0;
}
