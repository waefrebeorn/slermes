/* gate: tools_wake_word_py___looks_like_path — traced copy of ww_looks_like_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ww_looks_like_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ww_looks_like_path");
    bool result = ww_looks_like_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ww_looks_like_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ww_looks_like_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
