/* gate: utils_py__atomic_replace — traced copy of util_atomic_replace */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int util_atomic_replace(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("util_atomic_replace");
    int result = util_atomic_replace(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("util_atomic_replace");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s util_atomic_replace <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
