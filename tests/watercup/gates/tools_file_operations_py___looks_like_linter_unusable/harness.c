/* gate: tools_file_operations_py___looks_like_linter_unusable — traced copy of fo_looks_like_linter_unusable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int fo_looks_like_linter_unusable(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("fo_looks_like_linter_unusable");
    int result = fo_looks_like_linter_unusable(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_looks_like_linter_unusable");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_looks_like_linter_unusable <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
