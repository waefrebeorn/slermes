/* gate: tools_image_generation_tool_py__is_krea_model — traced copy of image_gen_is_krea_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool image_gen_is_krea_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("image_gen_is_krea_model");
    bool result = image_gen_is_krea_model(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("image_gen_is_krea_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s image_gen_is_krea_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
