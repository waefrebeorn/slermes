/* gate: gateway_platforms_base_py___release_platform_lock — traced copy of pb_release_platform_lock */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pb_release_platform_lock(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pb_release_platform_lock");
    int result = pb_release_platform_lock(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pb_release_platform_lock");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pb_release_platform_lock <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
