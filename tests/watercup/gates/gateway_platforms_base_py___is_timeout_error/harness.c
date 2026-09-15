/* gate: gateway_platforms_base_py___is_timeout_error — traced copy of pb_is_timeout_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool pb_is_timeout_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pb_is_timeout_error");
    bool result = pb_is_timeout_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pb_is_timeout_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pb_is_timeout_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
