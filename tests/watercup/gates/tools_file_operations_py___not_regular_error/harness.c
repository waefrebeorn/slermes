/* gate: tools_file_operations_py___not_regular_error — traced copy of fo_not_regular_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int fo_not_regular_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("fo_not_regular_error");
    int result = fo_not_regular_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_not_regular_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_not_regular_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
