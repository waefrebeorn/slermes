/* gate: gateway_platforms_base_py___path_lacks_deliverable_extension — traced copy of base_platform_path_lacks_deliverable_extension */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool base_platform_path_lacks_deliverable_extension(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("base_platform_path_lacks_deliverable_extension");
    bool result = base_platform_path_lacks_deliverable_extension(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("base_platform_path_lacks_deliverable_extension");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s base_platform_path_lacks_deliverable_extension <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
