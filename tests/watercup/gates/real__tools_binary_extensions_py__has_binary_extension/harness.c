/* gate: real__tools_binary_extensions_py__has_binary_extension — GAP sentinel for has_binary_extension */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "has_binary_extension";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s has_binary_extension <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("has_binary_extension");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("has_binary_extension");
    }
    return 0;
}
