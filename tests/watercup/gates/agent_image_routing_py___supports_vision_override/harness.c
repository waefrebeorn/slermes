/* gate: agent_image_routing_py___supports_vision_override — traced copy of imgr_supports_vision_override */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int imgr_supports_vision_override(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("imgr_supports_vision_override");
    int result = imgr_supports_vision_override(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("imgr_supports_vision_override");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s imgr_supports_vision_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
