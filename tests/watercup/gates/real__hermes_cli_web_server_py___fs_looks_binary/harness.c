/* gate: real__hermes_cli_web_server_py___fs_looks_binary — GAP sentinel for _fs_looks_binary */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_fs_looks_binary";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _fs_looks_binary <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_fs_looks_binary");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_fs_looks_binary");
    }
    return 0;
}
