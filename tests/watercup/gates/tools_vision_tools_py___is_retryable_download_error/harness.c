/* gate: tools_vision_tools_py___is_retryable_download_error — traced copy of vision_is_retryable_download_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool vision_is_retryable_download_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vision_is_retryable_download_error");
    bool result = vision_is_retryable_download_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vision_is_retryable_download_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vision_is_retryable_download_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
