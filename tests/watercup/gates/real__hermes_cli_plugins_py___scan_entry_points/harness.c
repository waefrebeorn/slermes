/* gate: real__hermes_cli_plugins_py___scan_entry_points — GAP sentinel for _scan_entry_points */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_scan_entry_points";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _scan_entry_points <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_scan_entry_points");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_scan_entry_points");
    }
    return 0;
}
