/* gate: agent_verification_evidence_py___clean_token — traced copy of vev_u_clean_token */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int vev_u_clean_token(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vev_u_clean_token");
    int result = vev_u_clean_token(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vev_u_clean_token");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vev_u_clean_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
