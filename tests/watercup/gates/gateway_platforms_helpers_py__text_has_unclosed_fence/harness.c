/* gate: gateway_platforms_helpers_py__text_has_unclosed_fence — traced copy of helpers_md_text_has_unclosed_fence */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool helpers_md_text_has_unclosed_fence(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("helpers_md_text_has_unclosed_fence");
    bool result = helpers_md_text_has_unclosed_fence(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("helpers_md_text_has_unclosed_fence");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s helpers_md_text_has_unclosed_fence <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
