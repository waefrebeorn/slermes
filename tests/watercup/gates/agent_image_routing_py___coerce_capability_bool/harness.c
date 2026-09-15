/* gate: agent_image_routing_py___coerce_capability_bool — traced copy of imgr_coerce_capability_bool */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int imgr_coerce_capability_bool(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("imgr_coerce_capability_bool");
    int result = imgr_coerce_capability_bool(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("imgr_coerce_capability_bool");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s imgr_coerce_capability_bool <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
