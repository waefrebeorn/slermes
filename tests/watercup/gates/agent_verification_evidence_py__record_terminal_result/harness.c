/* gate: agent_verification_evidence_py__record_terminal_result — traced copy of vev_record_terminal_result */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int vev_record_terminal_result(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vev_record_terminal_result");
    int result = vev_record_terminal_result(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vev_record_terminal_result");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vev_record_terminal_result <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
