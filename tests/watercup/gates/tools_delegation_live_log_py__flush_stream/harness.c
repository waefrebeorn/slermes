/* gate: tools_delegation_live_log_py__flush_stream — traced copy of dll_flush_stream */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dll_flush_stream(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dll_flush_stream");
    int result = dll_flush_stream(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dll_flush_stream");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dll_flush_stream <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
