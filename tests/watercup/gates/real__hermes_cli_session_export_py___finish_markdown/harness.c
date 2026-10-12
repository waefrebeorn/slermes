/* gate: real__hermes_cli_session_export_py___finish_markdown — GAP sentinel for _finish_markdown */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_finish_markdown";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _finish_markdown <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_finish_markdown");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_finish_markdown");
    }
    return 0;
}
