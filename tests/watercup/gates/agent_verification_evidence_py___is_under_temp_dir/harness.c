/* gate: agent_verification_evidence_py___is_under_temp_dir — traced copy of verify_is_under_temp_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool verify_is_under_temp_dir(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("verify_is_under_temp_dir");
    bool result = verify_is_under_temp_dir(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("verify_is_under_temp_dir");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s verify_is_under_temp_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
