/* gate: agent_credential_pool_py___normalize_pool_priorities — traced copy of cpl_normalize_pool_priorities */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cpl_normalize_pool_priorities(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cpl_normalize_pool_priorities");
    bool result = cpl_normalize_pool_priorities(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cpl_normalize_pool_priorities");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cpl_normalize_pool_priorities <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
