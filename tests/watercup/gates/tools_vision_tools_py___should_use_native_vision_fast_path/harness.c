/* gate: tools_vision_tools_py___should_use_native_vision_fast_path — traced copy of vi_should_use_native_vision_fast_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool vi_should_use_native_vision_fast_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vi_should_use_native_vision_fast_path");
    bool result = vi_should_use_native_vision_fast_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vi_should_use_native_vision_fast_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vi_should_use_native_vision_fast_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
