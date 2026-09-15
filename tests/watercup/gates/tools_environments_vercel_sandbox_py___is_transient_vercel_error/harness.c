/* gate: tools_environments_vercel_sandbox_py___is_transient_vercel_error — traced copy of ve_is_transient_vercel_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ve_is_transient_vercel_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ve_is_transient_vercel_error");
    bool result = ve_is_transient_vercel_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ve_is_transient_vercel_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ve_is_transient_vercel_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
