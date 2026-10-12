/* gate: real__agent_file_safety_py__build_write_denied_prefixes — GAP sentinel for build_write_denied_prefixes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "build_write_denied_prefixes";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s build_write_denied_prefixes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("build_write_denied_prefixes");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("build_write_denied_prefixes");
    }
    return 0;
}
