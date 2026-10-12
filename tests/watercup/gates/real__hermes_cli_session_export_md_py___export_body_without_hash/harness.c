/* gate: real__hermes_cli_session_export_md_py___export_body_without_hash — GAP sentinel for _export_body_without_hash */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_export_body_without_hash";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _export_body_without_hash <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_export_body_without_hash");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_export_body_without_hash");
    }
    return 0;
}
