/* gate: agent_nous_rate_guard_py__is_genuine_nous_rate_limit — traced copy of nrg_is_genuine_nous_rate_limit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool nrg_is_genuine_nous_rate_limit(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nrg_is_genuine_nous_rate_limit");
    bool result = nrg_is_genuine_nous_rate_limit(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nrg_is_genuine_nous_rate_limit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nrg_is_genuine_nous_rate_limit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
