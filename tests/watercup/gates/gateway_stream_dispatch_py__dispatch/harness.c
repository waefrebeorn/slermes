/* gate: gateway_stream_dispatch_py__dispatch — traced copy of std_dispatch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int std_dispatch(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("std_dispatch");
    int result = std_dispatch(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("std_dispatch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s std_dispatch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
