/* gate: tools_vision_tools_py___validate_image_url_async — traced copy of vi_validate_image_url_async */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool vi_validate_image_url_async(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vi_validate_image_url_async");
    bool result = vi_validate_image_url_async(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vi_validate_image_url_async");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vi_validate_image_url_async <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
