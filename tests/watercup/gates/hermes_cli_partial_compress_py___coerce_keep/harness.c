/* gate: hermes_cli_partial_compress_py___coerce_keep — traced copy of cmd_compress_coerce_keep_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cmd_compress_coerce_keep_value(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cmd_compress_coerce_keep_value");
    int result = cmd_compress_coerce_keep_value(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cmd_compress_coerce_keep_value");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cmd_compress_coerce_keep_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
