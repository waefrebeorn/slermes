/* gate: write_approval___check — traced copy of approval_check */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int approval_check(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("approval_check");
    int result = approval_check(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("approval_check");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s approval_check <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
