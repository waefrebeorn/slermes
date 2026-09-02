#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "libjson/json.h"
#include <stdbool.h>

extern bool aa_is_claude_model(const char *model);

static void trace_one(const char *input) {
    WCUP_BEGIN("aa_is_claude_model");
    bool result = aa_is_claude_model(input);
    const char *val = result ? "true" : "false";
    WCUP_VALSTR(val);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aa_is_claude_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *input = strcmp(argv[i], "NULL") == 0 ? NULL : argv[i];
        trace_one(input);
    }
    return 0;
}
