/* gate: real__tools_microsoft_graph_client_py__from_env — GAP sentinel for from_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "from_env";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s from_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("from_env");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("from_env");
    }
    return 0;
}
