/* gate: agent_verification_evidence_py__classify_verification_command — traced copy of vev_classify_verification_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int vev_classify_verification_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vev_classify_verification_command");
    int result = vev_classify_verification_command(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vev_classify_verification_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vev_classify_verification_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
