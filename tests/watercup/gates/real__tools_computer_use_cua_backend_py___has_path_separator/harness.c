/* gate: real__tools_computer_use_cua_backend_py___has_path_separator — GAP sentinel for _has_path_separator */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_has_path_separator";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _has_path_separator <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_has_path_separator");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_has_path_separator");
    }
    return 0;
}
