/* gate: agent_auxiliary_client_py___compression_threshold_for_model — traced copy of auxc_compression_threshold_for_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auxc_compression_threshold_for_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auxc_compression_threshold_for_model");
    int result = auxc_compression_threshold_for_model(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auxc_compression_threshold_for_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auxc_compression_threshold_for_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
