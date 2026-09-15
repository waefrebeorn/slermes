/* gate: gateway_session_py__sanitize_model_override — traced copy of sanitize_model_override */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool sanitize_model_override(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sanitize_model_override");
    bool result = sanitize_model_override(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sanitize_model_override");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sanitize_model_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
