/* gate: gateway_platforms_base_py___path_is_within — traced copy of base_platform_path_is_within */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int base_platform_path_is_within(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("base_platform_path_is_within");
    int result = base_platform_path_is_within(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("base_platform_path_is_within");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s base_platform_path_is_within <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
