/* gate: tools_file_operations_py___is_likely_binary — traced copy of fo_is_likely_binary */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int fo_is_likely_binary(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("fo_is_likely_binary");
    int result = fo_is_likely_binary(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_is_likely_binary");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_is_likely_binary <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
