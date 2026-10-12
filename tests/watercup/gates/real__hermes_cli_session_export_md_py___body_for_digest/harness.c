/* gate: real__hermes_cli_session_export_md_py___body_for_digest — GAP sentinel for _body_for_digest */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_body_for_digest";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _body_for_digest <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_body_for_digest");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_body_for_digest");
    }
    return 0;
}
