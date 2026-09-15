/* gate: tools_memory_tool_py___write_file — traced copy of memt_write_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int memt_write_file(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("memt_write_file");
    int result = memt_write_file(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("memt_write_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s memt_write_file <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
