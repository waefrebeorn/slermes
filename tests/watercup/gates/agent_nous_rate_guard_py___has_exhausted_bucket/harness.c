/* gate: agent_nous_rate_guard_py___has_exhausted_bucket — traced copy of nrg_has_exhausted_bucket */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool nrg_has_exhausted_bucket(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nrg_has_exhausted_bucket");
    bool result = nrg_has_exhausted_bucket(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nrg_has_exhausted_bucket");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nrg_has_exhausted_bucket <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
