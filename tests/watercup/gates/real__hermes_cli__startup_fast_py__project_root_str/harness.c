/* gate: real__hermes_cli__startup_fast_py__project_root_str — GAP sentinel for project_root_str */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "project_root_str";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s project_root_str <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("project_root_str");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("project_root_str");
    }
    return 0;
}
