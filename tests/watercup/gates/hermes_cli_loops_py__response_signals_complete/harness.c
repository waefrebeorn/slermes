/* gate: hermes_cli_loops_py__response_signals_complete — traced copy of loops_response_signals_complete */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int loops_response_signals_complete(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("loops_response_signals_complete");
    int result = loops_response_signals_complete(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("loops_response_signals_complete");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s loops_response_signals_complete <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
