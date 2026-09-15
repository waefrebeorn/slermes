/* gate: agent_verification_evidence_py___looks_like_target — traced copy of verify_looks_like_target */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool verify_looks_like_target(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("verify_looks_like_target");
    bool result = verify_looks_like_target(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("verify_looks_like_target");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s verify_looks_like_target <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
