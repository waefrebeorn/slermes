/* gate: real__hermes_cli_plugins_py___remove_callback — GAP sentinel for _remove_callback */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_remove_callback";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _remove_callback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_remove_callback");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_remove_callback");
    }
    return 0;
}
