/* gate: real__hermes_cli_linux_desktop_entry_py___remove_stale_scalable_icon — GAP sentinel for _remove_stale_scalable_icon */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_remove_stale_scalable_icon";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _remove_stale_scalable_icon <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_remove_stale_scalable_icon");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_remove_stale_scalable_icon");
    }
    return 0;
}
