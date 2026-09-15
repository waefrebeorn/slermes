/* gate: tools_transcription_tools_py___safe_find_spec — traced copy of trt_safe_find_spec */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool trt_safe_find_spec(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("trt_safe_find_spec");
    bool result = trt_safe_find_spec(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("trt_safe_find_spec");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s trt_safe_find_spec <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
