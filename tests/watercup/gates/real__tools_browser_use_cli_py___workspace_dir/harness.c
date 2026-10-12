/* gate: real__tools_browser_use_cli_py___workspace_dir — GAP sentinel for _workspace_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_workspace_dir";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _workspace_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_workspace_dir");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_workspace_dir");
    }
    return 0;
}
