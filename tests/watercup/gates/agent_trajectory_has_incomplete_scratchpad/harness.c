#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "libjson/json.h"

extern bool has_incomplete_scratchpad(const char *content);

static void trace_one(const char *input) {
    WCUP_BEGIN("has_incomplete_scratchpad");
    int result = has_incomplete_scratchpad(input);
    const char *val = result ? "true" : "false";
    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("has_incomplete_scratchpad");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ...\\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
