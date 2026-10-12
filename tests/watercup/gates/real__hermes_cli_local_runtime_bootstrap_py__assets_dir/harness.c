/* gate: real__hermes_cli_local_runtime_bootstrap_py__assets_dir — GAP sentinel for assets_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "assets_dir";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s assets_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("assets_dir");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("assets_dir");
    }
    return 0;
}
