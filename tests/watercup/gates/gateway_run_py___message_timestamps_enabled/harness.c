/* gate: gateway_run_py___message_timestamps_enabled — traced copy of gw_message_timestamps_enabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_message_timestamps_enabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_message_timestamps_enabled");
    bool result = gw_message_timestamps_enabled(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_message_timestamps_enabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_message_timestamps_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
