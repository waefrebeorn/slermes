/* gate: hermes_cli_setup_py__setup_telemetry — traced copy of setup_telemetry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int setup_telemetry(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("setup_telemetry");
    int result = setup_telemetry(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("setup_telemetry");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s setup_telemetry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
