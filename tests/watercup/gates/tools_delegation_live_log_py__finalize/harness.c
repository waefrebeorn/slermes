/* gate: tools_delegation_live_log_py__finalize — traced copy of dll_finalize */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dll_finalize(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dll_finalize");
    int result = dll_finalize(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dll_finalize");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dll_finalize <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
