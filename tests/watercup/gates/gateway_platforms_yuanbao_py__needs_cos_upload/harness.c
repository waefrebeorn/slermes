/* gate: gateway_platforms_yuanbao_py__needs_cos_upload — traced copy of yb_needs_cos_upload */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_needs_cos_upload(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_needs_cos_upload");
    int result = yb_needs_cos_upload(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_needs_cos_upload");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_needs_cos_upload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
