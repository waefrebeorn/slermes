/* gate: gateway_run_py___consume_clean_shutdown_marker — traced copy of gw_consume_clean_shutdown_marker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_consume_clean_shutdown_marker(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_consume_clean_shutdown_marker");
    int result = gw_consume_clean_shutdown_marker(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_consume_clean_shutdown_marker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_consume_clean_shutdown_marker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
