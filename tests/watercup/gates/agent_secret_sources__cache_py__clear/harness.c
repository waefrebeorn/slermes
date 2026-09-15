/* gate: agent_secret_sources__cache_py__clear — traced copy of scc_clear */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int scc_clear(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("scc_clear");
    int result = scc_clear(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scc_clear");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scc_clear <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
