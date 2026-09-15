/* gate: gateway_session_py___ensure_loaded — traced copy of gss_ensure_loaded */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gss_ensure_loaded(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gss_ensure_loaded");
    int result = gss_ensure_loaded(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gss_ensure_loaded");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gss_ensure_loaded <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
