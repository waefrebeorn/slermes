/* gate: gateway_platforms_yuanbao_py__build_forward_text — traced copy of yb_build_forward_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_build_forward_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_build_forward_text");
    int result = yb_build_forward_text(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_build_forward_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_build_forward_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
