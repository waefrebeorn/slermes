/* gate: utils_py___restore_file_owner — traced copy of util_restore_file_owner */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int util_restore_file_owner(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("util_restore_file_owner");
    int result = util_restore_file_owner(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("util_restore_file_owner");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s util_restore_file_owner <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
