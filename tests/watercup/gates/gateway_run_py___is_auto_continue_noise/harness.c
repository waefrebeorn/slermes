/* gate: gateway_run_py___is_auto_continue_noise — traced copy of gw_is_auto_continue_noise */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_is_auto_continue_noise(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_is_auto_continue_noise");
    bool result = gw_is_auto_continue_noise(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_is_auto_continue_noise");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_is_auto_continue_noise <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
