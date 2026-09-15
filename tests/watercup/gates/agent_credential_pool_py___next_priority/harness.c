/* gate: agent_credential_pool_py___next_priority — traced copy of cpl_next_priority */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cpl_next_priority(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cpl_next_priority");
    int result = cpl_next_priority(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cpl_next_priority");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cpl_next_priority <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
