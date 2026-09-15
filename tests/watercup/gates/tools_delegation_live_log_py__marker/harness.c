/* gate: tools_delegation_live_log_py__marker — traced copy of dll_marker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dll_marker(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("dll_marker");
    int result = dll_marker(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dll_marker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dll_marker <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
