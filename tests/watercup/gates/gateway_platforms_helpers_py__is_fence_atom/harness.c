/* gate: gateway_platforms_helpers_py__is_fence_atom — traced copy of helpers_md_is_fence_atom */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool helpers_md_is_fence_atom(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("helpers_md_is_fence_atom");
    bool result = helpers_md_is_fence_atom(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("helpers_md_is_fence_atom");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s helpers_md_is_fence_atom <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
