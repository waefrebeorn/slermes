/* gate: gateway_delivery_py___looks_like_int — traced copy of looks_like_int */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool looks_like_int(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("looks_like_int");
    bool result = looks_like_int(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("looks_like_int");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s looks_like_int <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
