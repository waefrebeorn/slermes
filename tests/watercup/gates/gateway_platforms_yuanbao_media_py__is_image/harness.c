/* gate: gateway_platforms_yuanbao_media_py__is_image — traced copy of ybm_is_image */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ybm_is_image(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ybm_is_image");
    int result = ybm_is_image(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ybm_is_image");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ybm_is_image <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
