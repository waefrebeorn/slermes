/* gate: gateway_run_py___session_has_compression_in_flight — traced copy of grun_u_session_has_compression_in_flight */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_session_has_compression_in_flight(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_session_has_compression_in_flight");
    int result = grun_u_session_has_compression_in_flight(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_session_has_compression_in_flight");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_session_has_compression_in_flight <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
