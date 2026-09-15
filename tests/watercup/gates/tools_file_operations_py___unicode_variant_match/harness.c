/* gate: tools_file_operations_py___unicode_variant_match — traced copy of _unicode_variant_match */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _unicode_variant_match(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_unicode_variant_match");
    int result = _unicode_variant_match(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_unicode_variant_match");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _unicode_variant_match <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
