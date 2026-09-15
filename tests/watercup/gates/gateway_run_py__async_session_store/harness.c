/* gate: gateway_run_py__async_session_store — traced copy of grun_async_session_store */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_async_session_store(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_async_session_store");
    int result = grun_async_session_store(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_async_session_store");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_async_session_store <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
