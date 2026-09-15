/* gate: gateway_platforms_base_py___path_under_denied_prefix — traced copy of base_platform_path_under_denied_prefix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool base_platform_path_under_denied_prefix(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("base_platform_path_under_denied_prefix");
    bool result = base_platform_path_under_denied_prefix(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("base_platform_path_under_denied_prefix");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s base_platform_path_under_denied_prefix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
