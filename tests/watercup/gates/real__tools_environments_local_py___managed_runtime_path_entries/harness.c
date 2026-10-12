/* gate: real__tools_environments_local_py___managed_runtime_path_entries — GAP sentinel for _managed_runtime_path_entries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_managed_runtime_path_entries";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _managed_runtime_path_entries <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_managed_runtime_path_entries");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_managed_runtime_path_entries");
    }
    return 0;
}
