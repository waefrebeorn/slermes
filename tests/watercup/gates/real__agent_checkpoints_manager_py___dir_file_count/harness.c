/* gate: real__tools_checkpoint_manager_py___dir_file_count — GAP sentinel for _dir_file_count */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_dir_file_count";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _dir_file_count <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_dir_file_count");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_dir_file_count");
    }
    return 0;
}
