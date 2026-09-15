/* gate: cli_py__is_table_divider — traced copy of cl2_is_table_divider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cl2_is_table_divider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cl2_is_table_divider");
    bool result = cl2_is_table_divider(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cl2_is_table_divider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cl2_is_table_divider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
