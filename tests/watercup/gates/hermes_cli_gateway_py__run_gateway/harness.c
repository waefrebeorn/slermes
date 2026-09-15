/* gate: hermes_cli_gateway_py__run_gateway — traced copy of run_gateway */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int run_gateway(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("run_gateway");
    int result = run_gateway(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("run_gateway");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s run_gateway <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
