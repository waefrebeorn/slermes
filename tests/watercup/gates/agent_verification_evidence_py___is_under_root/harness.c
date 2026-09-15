/* gate: agent_verification_evidence_py___is_under_root — traced copy of verify_is_under_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int verify_is_under_root(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("verify_is_under_root");
    int result = verify_is_under_root(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("verify_is_under_root");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s verify_is_under_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
