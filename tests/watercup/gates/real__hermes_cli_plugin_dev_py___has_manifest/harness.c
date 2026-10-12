/* gate: real__hermes_cli_plugin_dev_py___has_manifest — GAP sentinel for _has_manifest */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_has_manifest";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _has_manifest <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_has_manifest");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_has_manifest");
    }
    return 0;
}
