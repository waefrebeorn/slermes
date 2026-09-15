/* gate: tools_vision_tools_py___image_url_shape_ok — traced copy of vision_validate_image_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool vision_validate_image_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vision_validate_image_path");
    bool result = vision_validate_image_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vision_validate_image_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vision_validate_image_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
