/* gate: real__hermes_cli_archive_safe_py__make_targz — GAP sentinel for make_targz */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "make_targz";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s make_targz <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("make_targz");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("make_targz");
    }
    return 0;
}
