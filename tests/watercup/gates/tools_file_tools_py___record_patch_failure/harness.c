/* gate: tools_file_tools_py___record_patch_failure — traced copy of file_tools_record_patch_failure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int file_tools_record_patch_failure(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("file_tools_record_patch_failure");
    int result = file_tools_record_patch_failure(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("file_tools_record_patch_failure");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_tools_record_patch_failure <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
