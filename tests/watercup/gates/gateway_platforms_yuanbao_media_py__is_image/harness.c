/* gate: gateway_platforms_yuanbao_media_py__is_image — traced copy of ybm_is_image */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ybm_is_image(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("ybm_is_image");
    int result = ybm_is_image(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ybm_is_image");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ybm_is_image <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
