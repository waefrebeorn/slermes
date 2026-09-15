/* gate: utils_py__atomic_json_write — traced copy of util_atomic_json_write */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int util_atomic_json_write(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("util_atomic_json_write");
    int result = util_atomic_json_write(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("util_atomic_json_write");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s util_atomic_json_write <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
