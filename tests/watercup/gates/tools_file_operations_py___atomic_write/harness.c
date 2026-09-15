/* gate: tools_file_operations_py___atomic_write — traced copy of fo_atomic_write */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int fo_atomic_write(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("fo_atomic_write");
    int result = fo_atomic_write(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_atomic_write");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_atomic_write <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
