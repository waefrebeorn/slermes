/* gate: agent_verification_evidence_py___summarize_output — traced copy of vev_u_summarize_output */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int vev_u_summarize_output(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vev_u_summarize_output");
    int result = vev_u_summarize_output(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vev_u_summarize_output");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vev_u_summarize_output <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
