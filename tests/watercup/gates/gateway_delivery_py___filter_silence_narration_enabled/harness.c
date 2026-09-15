/* gate: gateway_delivery_py___filter_silence_narration_enabled — traced copy of filter_silence_narration_enabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool filter_silence_narration_enabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("filter_silence_narration_enabled");
    bool result = filter_silence_narration_enabled(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("filter_silence_narration_enabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s filter_silence_narration_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
