/* gate: gateway_platforms_base_py___is_retryable_error — traced copy of gw_base__is_retryable_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_base__is_retryable_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_base__is_retryable_error");
    int result = gw_base__is_retryable_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_base__is_retryable_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_base__is_retryable_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
