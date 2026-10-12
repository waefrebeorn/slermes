/* gate: real__hermes_cli_uninstall_py__log_info — GAP sentinel for log_info */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "log_info";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s log_info <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("log_info");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("log_info");
    }
    return 0;
}
