#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "libjson/json.h"

extern bool looks_like_table_row(const char *row);

static void trace_one(const char *input) {
    WCUP_BEGIN("looks_like_table_row");
    int result = looks_like_table_row(input);
    const char *val = result ? "true" : "false";
    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("looks_like_table_row");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ...\\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
