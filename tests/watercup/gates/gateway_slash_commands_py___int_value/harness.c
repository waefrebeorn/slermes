/* gate: gateway_slash_commands_py___int_value — traced copy of slash_int_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int slash_int_value(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("slash_int_value");
    int result = slash_int_value(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("slash_int_value");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s slash_int_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
