# include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "libjson/json.h"
#include <stdbool.h>

extern bool cl2_is_table_divider(const char *row);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_table_divider");
    bool result = cl2_is_table_divider(input);
    char val[64];
    snprintf(val, sizeof(val), "%s", result ? "true" : "false");
    WCUP_VALSTR(val);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_table_divider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ...\\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *input = strcmp(argv[i], "NULL") == 0 ? NULL : argv[i];
        trace_one(input);
    }
    return 0;
}