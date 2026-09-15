/* gate: gateway_run_py___is_transient_network_error — traced copy of gw_is_transient_network_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_is_transient_network_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_is_transient_network_error");
    bool result = gw_is_transient_network_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_is_transient_network_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_is_transient_network_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
