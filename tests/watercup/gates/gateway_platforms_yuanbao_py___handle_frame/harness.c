/* gate: gateway_platforms_yuanbao_py___handle_frame — traced copy of yb2_handle_frame */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb2_handle_frame(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb2_handle_frame");
    int result = yb2_handle_frame(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb2_handle_frame");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb2_handle_frame <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
