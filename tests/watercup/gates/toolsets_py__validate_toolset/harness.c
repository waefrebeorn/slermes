/* gate: toolsets_py__validate_toolset — traced copy of toolsets_validate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool toolsets_validate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("toolsets_validate");
    bool result = toolsets_validate(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("toolsets_validate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s toolsets_validate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
